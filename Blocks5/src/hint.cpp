#include "pch.h"
#include "hint.h"
#include "player.h"
#include "engine.h"
#include "font.h"
#include "texture.h"

namespace
{
	// The note at full size: the picture measures 300x400 and the text starts
	// a little way inside it. The texture both are drawn into together is a
	// power of two - WebGL 1 can only handle others with restrictions, and the
	// sheet fits comfortably inside.
	const int NOTE_WIDTH = 300;
	const int NOTE_HEIGHT = 400;

	// Two panels side by side: on the left the sheet with the text, on the
	// right the same without - only the front is written on. The gap is
	// generous enough that no texel of one panel bleeds into the other.
	const int NOTE_TEXTURE_W = 1024;
	const int NOTE_TEXTURE_H = 512;
	const int BACK_PANEL_X = 512;
	const int TEXT_LEFT = 35;
	const int TEXT_TOP = 45;
	const int TEXT_WIDTH = 230;

	// The shadow lies underneath, offset by five pixels.
	const int SHADOW_OFFSET = 5;
	const double SHADOW_ALPHA = 0.3;

	// How the note rolls up. ROLL_LENGTH is the fraction of the sheet that is
	// rolled in at the top and the bottom while it flies in; ROLL_TURNS how
	// far it winds itself up in the process.
	//
	// Together the two fix the radius: ROLL_LENGTH * NOTE_HEIGHT /
	// (ROLL_TURNS * 2 * PI). A fatter bead at half a turn therefore means
	// rolling up more paper. 0.30 leaves the middle 40% of the sheet flat and
	// makes the radius 38 pixels.
	//
	// Half a turn is the limit, and that is not a matter of taste: up to there
	// every piece of paper on its way outward comes nearer to the viewer; the
	// roll can therefore be drawn back to front and covers itself correctly.
	// Beyond that the end would come round to the rear again, and without a
	// depth buffer - which this pass does not have - it would still lie on top.
	const double ROLL_LENGTH = 0.30;
	const double ROLL_TURNS = 0.50;

	// How fine. The rolls get the subdivision, the flat middle needs none:
	// there is nothing to curve there.
	const int ROLL_BANDS = 48;

	// Focal length in pixels, for the perspective divide done by hand: what
	// lies nearer the viewer gets bigger. Without it the roll would be nothing
	// but a squashed strip.
	const double PERSPECTIVE = 700.0;

	// And how far to the left of the axis the viewer stands. What comes
	// towards them therefore moves right - the slant the 16x16 sprite has as
	// well. The offset is e * (f - 1), which is what a laterally displaced eye
	// sees, and zero in the plane of the sheet (f = 1): the flat note stays
	// pixel on pixel.
	const double VIEW_OFFSET_X = 300.0;

	const double PI = 3.1415926535897932384626433832795;

	// How bright the paper stands where it shows the viewer its edge. What
	// counts is the surface normal, not the angle of rotation: on the back you
	// are looking at the other face, whose normal points back at the viewer,
	// hence |cos| and not cos. Fully turned towards them is 1.0, front and
	// back alike.
	const double SHADE_EDGE = 0.75;

	// When the note unrolls and how long it takes, both in logic ticks from
	// the moment the field is stepped onto. At 20 ticks it is at 96% of its
	// size - it has almost landed by the time the unrolling begins.
	const int UNROLL_START = 20;
	const int UNROLL_END = 40;

	// On leaving, the note rolls up again at the same speed it opened at. It
	// stays where it is until it is done (onUpdate); nothing hurries it.
	const int ROLL_UP_SPEED = 1;

	// From when the note counts as arrived and is rounded to whole pixels:
	// half a pixel over the screen diagonal of 800.
	const double SNAP_RESIDUAL = 0.5 / 800.0;

	// How far along the flight the note fades in and out; from there it is
	// fully opaque. Transparent paper with an opaque, unwritten back
	// contradicts itself, and there is nothing to see through it anyway.
	const double FADE_UNTIL = 0.5;

	// A point on the paper. py runs from 0 (top edge) to NOTE_HEIGHT.
	struct NotePoint
	{
		double y;       // position in the picture, from the note's centre
		double depth;   // how far in front of the sheet plane, nearer the viewer
		double shade;   // how bright the paper stands here
		bool back;      // is the back facing the viewer?
	};

	NotePoint rollPoint(double py,
						double unroll)
	{
		NotePoint p;
		p.y = py - 0.5 * NOTE_HEIGHT;
		p.depth = 0.0;
		p.shade = 1.0;
		p.back = false;

		const double rolled = ROLL_LENGTH * NOTE_HEIGHT * (1.0 - unroll);
		if(rolled < 1.0) return p;   // flat: nothing left to curve

		const double thetaMax = ROLL_TURNS * 2.0 * PI;
		const double radius = rolled / thetaMax;

		// Arc length from the crease where the paper lifts off the plane.
		double s = 0.0;
		double direction = 0.0;
		if(py < rolled)                      { s = rolled - py;                     direction = -1.0; }
		else if(py > NOTE_HEIGHT - rolled)   { s = py - (NOTE_HEIGHT - rolled);     direction = 1.0; }
		else return p;

		const double theta = s / radius;
		const double edge = direction * (0.5 * NOTE_HEIGHT - rolled);
		p.y = edge + direction * radius * sin(theta);

		// Top edge towards the viewer, bottom edge away from them - the way the
		// 16x16 sprite on the field shows it. direction is -1 at the top and +1
		// at the bottom.
		p.depth = -direction * radius * (1.0 - cos(theta));

		// Past the quarter turn the paper shows its back. At exactly that point
		// it stands edge-on, which is why the jump costs nothing.
		p.back = (theta > 0.5 * PI);
		p.shade = SHADE_EDGE + (1.0 - SHADE_EDGE) * fabs(cos(theta));
		return p;
	}
}

Hint::Hint(Level& level,
		   const Vec2i& position,
		   const std::string& text) : Object(level, 2)
{
	warpTo(position);
	flags = OF_FIXED | OF_TRANSPORTABLE | OF_COLLECTABLE;
	this->text = text;
	alpha = shownAlpha = 0.0;
	unroll = 0.0;
	activeTicks = 0;
	dismissed = false;
	noteTexture = 0;
	// Vec2i has no initialising default constructor.
	targetPosition = Vec2i(320, 200);

	p_sprite = level.getHint();
	p_font = level.getHintFont();

	Font::Options options = p_font->getOptions();
	options.charSpacing = -1;
	options.lineSpacing = 0.95;
	options.shadows = 1;
	p_font->setOptions(options);
}

Hint::~Hint()
{
	// The usual way is onRemove(): Level::removeObject() calls it, and
	// removeOldObjects() is the only place that ever deletes an Object - a
	// note cannot disappear without it having run. It stands here a second
	// time; handing the texture back must not hang on a single hook.
	//
	// Reaching the Engine is safe here because releaseNoteTexture() bails out
	// first when no texture is borrowed at all: only a note that has drawn in
	// a running level holds one, and that level belongs to a local variable of
	// main() - which falls before the static Engine.
	//
	// In the browser this destructor never runs anyway: mainLoop() does not
	// return there. The texture is handed back in onUpdate() as soon as the
	// note is invisible, and in onRemove() on a level change - both run during
	// play and not at shutdown.
	releaseNoteTexture();
}

void Hint::onRemove()
{
	Object::onRemove();
	releaseNoteTexture();
}

void Hint::releaseNoteTexture()
{
	if(!noteTexture) return;
	Engine::inst().releaseOffscreenTexture(noteTexture);
	noteTexture = 0;
	bakedText = "";
}

void Hint::updateSprites()
{
	// Note object
	sprites.add(Vec2i(96, 288));
}

void Hint::bakeNote()
{
	Engine& engine = Engine::inst();

	const std::string wanted = p_font->adjustText(localizeString(text), TEXT_WIDTH);
	if(noteTexture && wanted == bakedText) return;

	// Its own texture, not a shared one: on the step from one note to its
	// neighbour both are visible, and the one must not draw into the sheet the
	// other is reading from.
	const Vec2i size(NOTE_TEXTURE_W, NOTE_TEXTURE_H);
	const uint target = noteTexture ? noteTexture : engine.acquireOffscreenTexture(size);
	if(!target) return;   // no framebuffer object: then without one, see onRender()

	if(!engine.beginRenderToTexture(target, size))
	{
		if(!noteTexture) engine.releaseOffscreenTexture(target);
		return;
	}

	GLfloat oldClear[4];
	glGetFloatv(GL_COLOR_CLEAR_VALUE, oldClear);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glClearColor(oldClear[0], oldClear[1], oldClear[2], oldClear[3]);

	// The alpha channel has to be right, because the texture is itself blended
	// again in a moment: the colour arrives weighted (GL_SRC_ALPHA), the alpha
	// unweighted (GL_ONE). What comes out is premultiplied - and that is
	// exactly how it is drawn again below.
	engine.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	p_sprite->bind();
	engine.renderSprite(Vec2i(0, 0), Vec2i(0, 0), Vec2i(NOTE_WIDTH, NOTE_HEIGHT), Vec4d(1.0));

	// The same sheet once more beside it, this time without the text: that is
	// the back. Nothing is mirrored - the paper curls about a horizontal axis,
	// and left stays left.
	engine.renderSprite(Vec2i(BACK_PANEL_X, 0), Vec2i(0, 0), Vec2i(NOTE_WIDTH, NOTE_HEIGHT), Vec4d(1.0));

	p_font->renderText(wanted, Vec2i(TEXT_LEFT, TEXT_TOP), Vec4d(1.0));

	engine.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
	engine.endRenderToTexture();

	noteTexture = target;
	bakedText = wanted;
}

void Hint::renderNoteMesh(const Vec4d& color,
						  double unroll) const
{
	// Back to front, the only order there is without a depth buffer. The
	// bottom roll goes away to the rear, its outer end therefore lying
	// furthest back; the top one comes forward. Each splits at the quarter
	// turn into a front and a back section, and the seam has to fall on a
	// vertex - or one quad would drag its texture across both panels.
	const double uWidth = static_cast<double>(NOTE_WIDTH) / NOTE_TEXTURE_W;
	const double uBack = static_cast<double>(BACK_PANEL_X) / NOTE_TEXTURE_W;
	const double rolled = ROLL_LENGTH * NOTE_HEIGHT * (1.0 - unroll);
	const double flatTop = (rolled < 1.0) ? 0.0 : rolled;
	const double flatBottom = NOTE_HEIGHT - flatTop;

	// Arc up to the quarter turn: radius * PI/2, i.e. flatTop / (4*ROLL_TURNS).
	// Where the roll does not reach that far, the back drops out as an empty
	// section - as it does for the flat sheet, where flatTop is 0 itself.
	const double half = min(flatTop, flatTop / (ROLL_TURNS * 4.0));

	// [from, to] on the paper, how fine, and which panel of the texture.
	const int NUM_SECTIONS = 5;
	double sections[NUM_SECTIONS][2];
	int steps[NUM_SECTIONS];
	bool back[NUM_SECTIONS];
	sections[0][0] = NOTE_HEIGHT;         sections[0][1] = flatBottom + half; steps[0] = ROLL_BANDS / 2; back[0] = true;
	sections[1][0] = flatBottom + half;   sections[1][1] = flatBottom;        steps[1] = ROLL_BANDS / 2; back[1] = false;
	sections[2][0] = flatTop;             sections[2][1] = flatBottom;        steps[2] = 1;              back[2] = false;
	sections[3][0] = flatTop;             sections[3][1] = flatTop - half;    steps[3] = ROLL_BANDS / 2; back[3] = false;
	sections[4][0] = flatTop - half;      sections[4][1] = 0.0;               steps[4] = ROLL_BANDS / 2; back[4] = true;

	// A triangle strip and not GL_QUAD_STRIP: WebGL does not know that one, and
	// the web build hands the mode straight through (WebBuild/gl_immediate.cpp).
	// For a strip of quads the two are the same.
	for(int section = 0; section < NUM_SECTIONS; section++)
	{
		const double from = sections[section][0];
		const double to = sections[section][1];
		if(from == to) continue;

		const double u0 = back[section] ? uBack : 0.0;
		const double u1 = u0 + uWidth;

		glBegin(GL_TRIANGLE_STRIP);
		for(int k = 0; k <= steps[section]; k++)
		{
			const double py = from + (to - from) * k / steps[section];
			const NotePoint np = rollPoint(py, unroll);

			// Perspective divide by hand: what lies nearer gets bigger.
			const double f = PERSPECTIVE / (PERSPECTIVE - np.depth);
			const double x = 0.5 * NOTE_WIDTH * f;
			const double y = np.y * f;
			const double dx = VIEW_OFFSET_X * (f - 1.0);
			// The other way round: drawing used (0,0) at the TOP left, and a
			// texture starts at the bottom. In the texture the note stands
			// upside down, exactly like the game's own frame in the
			// framebuffer object.
			const double t = 1.0 - py / NOTE_TEXTURE_H;

			// Premultiplied: the colour already carries the alpha in itself,
			// and the vertex colour we paint with has to take it along.
			const double b = np.shade * color.a;
			glColor4d(color.r * b, color.g * b, color.b * b, color.a);
			glTexCoord2d(u0, t); glVertex2d(dx - x, y);
			glTexCoord2d(u1, t); glVertex2d(dx + x, y);
		}
		glEnd();
	}
}

void Hint::renderNote(const Vec4d& color,
					  double unroll) const
{
	Engine& engine = Engine::inst();

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, noteTexture);

	// This texture does not come from Texture::bind(): the pixel matrix of the
	// last image is still in place. Here the maths runs in 0..1.
	glMatrixMode(GL_TEXTURE);
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);

	// Blend premultiplied, because the texture came about that way.
	engine.setBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	// The shadow is the same paper in black, offset a little way.
	glPushMatrix();
	glTranslated(SHADOW_OFFSET, SHADOW_OFFSET, 0.0);
	renderNoteMesh(Vec4d(0.0, 0.0, 0.0, color.a * SHADOW_ALPHA), unroll);
	glPopMatrix();

	renderNoteMesh(color, unroll);

	engine.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);

	glMatrixMode(GL_TEXTURE);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);

	// Leave it as tidy as Texture::unbind() would: right afterwards the level
	// draws the flash, and that wants no texture.
	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_TEXTURE_2D);
}

void Hint::renderNoteFlat(const Vec4d& color,
						  double unroll) const
{
	// Without a framebuffer object there is no texture that note and text
	// could be drawn into together. Then the two go one after the other -
	// under the same matrix, so the writing at least flies with the paper
	// instead of appearing out of nowhere at the end.
	Engine& engine = Engine::inst();
	const Vec2i corner(-NOTE_WIDTH / 2, -NOTE_HEIGHT / 2);

	// Nothing rolls here, but the time for it passes all the same - with
	// nothing to see, that would look as if the game had hung. At least the
	// height shrinks to the part lying flat; the writing squashes with it, and
	// that is the price of this path.
	const double rolled = ROLL_LENGTH * NOTE_HEIGHT * (1.0 - unroll);
	const double radius = rolled / (ROLL_TURNS * 2.0 * PI);
	glPushMatrix();
	glScaled(1.0, ((NOTE_HEIGHT - 2.0 * rolled) + 2.0 * radius) / NOTE_HEIGHT, 1.0);

	p_sprite->bind();
	engine.renderSprite(corner + Vec2i(SHADOW_OFFSET, SHADOW_OFFSET), Vec2i(0, 0),
						Vec2i(NOTE_WIDTH, NOTE_HEIGHT),
						Vec4d(0.0, 0.0, 0.0, color.a * SHADOW_ALPHA));
	engine.renderSprite(corner, Vec2i(0, 0), Vec2i(NOTE_WIDTH, NOTE_HEIGHT), color);
	p_font->renderText(p_font->adjustText(localizeString(text), TEXT_WIDTH),
					   corner + Vec2i(TEXT_LEFT, TEXT_TOP), color);

	glPopMatrix();
}

void Hint::onRender(int layer,
					const Vec4d& color)
{
	if(layer == 1) Engine::inst().renderSprites(sprites, color);
	else if(layer == 42 || layer == 43)
	{
		double r = (0.85 - shownAlpha) * 45.0;
		double i = shownAlpha / 0.85;
		double s = i;
		double a = clamp(i / FADE_UNTIL, 0.0, 1.0);

		// Layer 43 is the preview in the level editor: fully unrolled, centred.
		// That is a display matter and must not change targetPosition -
		// otherwise the note points somewhere else in the game afterwards.
		//
		// The note comes rolled up only where the picture is a sheet of paper -
		// the space skin shows a display panel, and that does not roll. 1.0
		// means flat: then both rolls drop out of renderNoteMesh() and leave
		// the single quad.
		Vec2i target = targetPosition;
		double shownUnroll = level.isHintScroll() ? unroll : 1.0;
		if(layer == 43) a = 1.0, r = 0.0, i = 1.0, s = 1.0, target = Vec2i(320, 200), shownUnroll = 1.0;

		// Arrived means exactly arrived: shownAlpha only approaches 0.85, and
		// scale, angle and position would stay fractions off for ever, with
		// GL_LINEAR mixing every texel of the baked text out of two. Below
		// SNAP_RESIDUAL they are therefore rounded to exactly 1, exactly 0 and
		// exactly targetPosition - which is a Vec2i, and the corners of the
		// strip in renderNoteMesh() are whole numbers anyway.
		if(1.0 - i < SNAP_RESIDUAL) i = 1.0, s = 1.0, r = 0.0;

		// a, and not shownAlpha: the editor forces a to 1 above and drives
		// shownAlpha not at all, since it never runs Level::update().
		if(a > 1.0 / 255.0)
		{
			// Sheet and writing bake into one texture, so the writing turns and
			// rolls up with the paper; re-made whenever the text changes.
			bakeNote();

			glPushMatrix();
			Vec2i p = -getShownPositionInPixels();
			glTranslated(p.x, p.y, 0.0);

			Vec4d realColor(color.r, color.g, color.b, color.a * a);

			glPushMatrix();
			Vec2d sp = (1.0 - i) * static_cast<Vec2d>(getShownPositionInPixels()) + i * static_cast<Vec2d>(target);
			glTranslated(sp.x, sp.y, 0.0);
			glScaled(s, s, 1.0);
			glRotated(r, 0.0, 0.0, 1.0);

			if(noteTexture) renderNote(realColor, shownUnroll);
			else renderNoteFlat(realColor, shownUnroll);

			glPopMatrix();
			glPopMatrix();
		}
	}
}

void Hint::onUpdate()
{
	// Player here?
	Object* p_obj = level.getFrontObjectAt(position);
	const bool playerIsHere = (p_obj == level.getActivePlayer());

	// A player who walks away has no longer dismissed the note: it opens
	// again the next time the field is stepped onto.
	if(!playerIsHere) dismissed = false;
	const bool open = playerIsHere && !dismissed;

	// The target is decided once, in the tick the note opens. Any later look
	// at the player position would make it jump to the other side of the
	// screen as the player steps off the field - exactly while it is
	// disappearing. Not in onCollect(): that only runs once the player stands
	// at the centre, and by then the note is already on its way.
	if(open && activeTicks == 0) updateTargetPosition();

	// The unrolling runs by the clock and not by shownAlpha: that only
	// approaches its target and would never quite arrive, leaving the note a
	// little rolled up for ever.
	if(open) { if(activeTicks < UNROLL_END) activeTicks++; }
	else     { activeTicks = max(0, activeTicks - ROLL_UP_SPEED); }
	unroll = clamp(static_cast<double>(activeTicks - UNROLL_START) /
				   (UNROLL_END - UNROLL_START), 0.0, 1.0);

	// Roll up first, then disappear - hence the rolling above. While anything
	// is still left to roll up, the note stays fully visible and in place.
	alpha = (open || unroll > 0.0) ? 0.85 : 0.0;
	shownAlpha = 0.15 * alpha + 0.85 * shownAlpha;
	if(shownAlpha <= 1.0 / 255.0)
	{
		shownAlpha = 0.0;
		releaseNoteTexture();
	}
}

void Hint::updateTargetPosition()
{
	Player* p_player = level.getActivePlayer();
	if(!p_player) return;

	// The note must not cover the player.
	targetPosition = Vec2i(320, 200);
	const Vec2i pp = p_player->getPosition() * 16;
	if(pp.x >= 140 && pp.x <= 490)
	{
		if(pp.x < 320) targetPosition.x = 470;
		else targetPosition.x = 170;
	}
}

void Hint::onCollect(Player* p_player)
{
	// Deliberately empty, and that is the only reason it is here at all:
	// Object::onCollect() would make the note disappear. It stays lying there
	// and can be read again.
}

bool Hint::dismiss()
{
	// Only when there is anything to see at all. Otherwise the note reports
	// nothing and Escape opens the game menu as always. activeTicks rather
	// than shownAlpha, because the note is still fading out between two
	// visits - and that is nothing anybody could close.
	if(dismissed || activeTicks <= 0) return false;

	dismissed = true;
	return true;
}

void Hint::saveAttributes(TiXmlElement* p_target)
{
	TiXmlElement* p_text = new TiXmlElement("Text");
	TiXmlText* p_data = new TiXmlText(text);
	p_data->SetCDATA(true);
	p_text->LinkEndChild(p_data);
	p_target->LinkEndChild(p_text);
}

const std::string& Hint::getText() const
{
	return text;
}

void Hint::setText(const std::string& text)
{
	this->text = text;
}
