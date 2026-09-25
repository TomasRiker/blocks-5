#include "pch.h"
#include "hint.h"
#include "player.h"
#include "engine.h"
#include "font.h"
#include "texture.h"
#include "sound.h"
#include "soundinstance.h"

namespace
{
	// The note at full size, as the picture measures it.
	const int NOTE_WIDTH = 300;
	const int NOTE_HEIGHT = 400;

	// The baked texture, a power of two because WebGL 1 restricts the others.
	// Two panels side by side: the sheet with the text at the left, the same
	// sheet unwritten at BACK_PANEL_X as its back, far enough apart that no
	// texel of one bleeds into the other. TEXT_* place the writing.
	const int NOTE_TEXTURE_W = 1024;
	const int NOTE_TEXTURE_H = 512;
	const int BACK_PANEL_X = 512;
	const int TEXT_LEFT = 35;
	const int TEXT_TOP = 45;
	const int TEXT_WIDTH = 230;

	const int SHADOW_OFFSET = 5;
	const float SHADOW_ALPHA = 0.3f;

	// ROLL_LENGTH is the fraction of the sheet rolled in at the top and at
	// the bottom while the note flies in, ROLL_TURNS how far each roll winds.
	// Fully rolled, the middle 40% is flat and the radius is 38 pixels:
	// ROLL_LENGTH * NOTE_HEIGHT / (ROLL_TURNS * 2 * PI). Half a turn is a hard
	// limit, not taste: up to there the depth changes monotonically along
	// each roll, so drawing it in order paints it back to front. Beyond it
	// the end curls round behind again, and with no depth test it would be
	// drawn on top.
	const float ROLL_LENGTH = 0.30f;
	const float ROLL_TURNS = 0.50f;

	// Bands per roll. The flat middle is a single band.
	const int ROLL_BANDS = 48;

	// Focal length in pixels for the perspective divide done by hand, which
	// is what makes the roll read as a cylinder and not a squashed strip.
	const float PERSPECTIVE = 700.0f;

	// How far left of the axis the eye stands, so that what comes towards it
	// moves right - the slant the 16x16 sprite has. The shift is
	// VIEW_OFFSET_X * (f - 1), zero in the plane of the sheet (f = 1), so the
	// flat note stays pixel on pixel.
	const float VIEW_OFFSET_X = 300.0f;

	const float PI = 3.1415926535897932384626433832795f;

	// Brightness of the paper seen edge-on, rising to 1 where it faces the
	// viewer. |cos| and not cos: past the quarter turn the viewer sees the
	// back, whose normal faces them again.
	const float SHADE_EDGE = 0.75f;

	// When the note unrolls and how long it takes, both in logic ticks from
	// the moment the field is stepped onto. At 20 ticks it is at 96% of its
	// size - it has almost landed by the time the unrolling begins.
	const int UNROLL_START = 20;
	const int UNROLL_END = 40;

	// How fast a rustle fades when its motion is cut short: the fraction of
	// the remaining volume taken off each logic tick. At 0.3 it halves every
	// two ticks, is inaudible after seven (140 ms) and ends by the thirteenth.
	const float SCROLL_FADE_SPEED = 0.3f;

	// On leaving, the note rolls up again at the same speed it opened at. It
	// stays where it is until it is done (onUpdate); nothing hurries it.
	const int ROLL_UP_SPEED = 1;

	// From when the note counts as arrived and is rounded to whole pixels:
	// half a pixel over the screen diagonal of 800.
	const float SNAP_RESIDUAL = 0.5f / 800.0f;

	// How far along the flight the note fades in and out; beyond that it is
	// opaque, since transparent paper with an opaque, unwritten back would
	// contradict itself.
	const float FADE_UNTIL = 0.5f;

	// A point on the paper, as rollPoint() finds it for py, which runs from 0
	// (top edge) to NOTE_HEIGHT.
	struct NotePoint
	{
		float y;       // position in the picture, from the note's centre
		float depth;   // how far in front of the sheet plane, nearer the viewer
		float shade;   // how bright the paper stands here
		bool back;     // is the back facing the viewer?
	};

	NotePoint rollPoint(float py,
						float unroll)
	{
		NotePoint p;
		p.y = py - 0.5f * NOTE_HEIGHT;
		p.depth = 0.0f;
		p.shade = 1.0f;
		p.back = false;

		const float rolled = ROLL_LENGTH * NOTE_HEIGHT * (1.0f - unroll);
		if(rolled < 1.0f) return p;   // flat: nothing left to curve

		const float thetaMax = ROLL_TURNS * 2.0f * PI;
		const float radius = rolled / thetaMax;

		// Arc length from the crease where the paper lifts off the plane.
		float s = 0.0f;
		float direction = 0.0f;
		if(py < rolled)                      { s = rolled - py;                     direction = -1.0f; }
		else if(py > NOTE_HEIGHT - rolled)   { s = py - (NOTE_HEIGHT - rolled);     direction = 1.0f; }
		else return p;

		const float theta = s / radius;
		const float edge = direction * (0.5f * NOTE_HEIGHT - rolled);
		p.y = edge + direction * radius * sinf(theta);

		// Top edge towards the viewer, bottom edge away from them - the way the
		// 16x16 sprite on the field shows it. direction is -1 at the top and +1
		// at the bottom.
		p.depth = -direction * radius * (1.0f - cosf(theta));

		// Past the quarter turn the paper shows its back. At exactly that point
		// it stands edge-on, which is why the jump costs nothing.
		p.back = (theta > 0.5f * PI);
		p.shade = SHADE_EDGE + (1.0f - SHADE_EDGE) * fabsf(cosf(theta));
		return p;
	}
}

Hint::Hint(Level& level,
		   const Vec2i& position,
		   const std::string& text) : Object(level, 2)
{
	renderLayers = RL_MAIN | RL_OVERLAY | RL_HINT_PREVIEW;
	warpTo(position);
	flags = OF_FIXED | OF_TRANSPORTABLE | OF_COLLECTABLE;
	this->text = text;
	alpha = shownAlpha = 0.0f;
	unroll = 0.0f;
	activeTicks = 0;
	dismissed = false;
	noteTexture = 0;
	p_scrollSound = 0;
	scrollDirection = 0;
	// Vec2i has no initialising default constructor.
	targetPosition = Vec2i(320, 200);

	p_sprite = level.getHint();
	p_font = level.getHintFont();

	Font::Options options = p_font->getOptions();
	options.charSpacing = -1;
	options.lineSpacing = 0.95f;
	options.shadows = 1;
	p_font->setOptions(options);
}

Hint::~Hint()
{
	// onRemove() normally hands the texture back; this is the second belt.
	// Reaching the Engine from here is safe: releaseNoteTexture() returns at
	// once when nothing is borrowed, the Engine is a static that outlives
	// every level, and a hand-back after Engine::exit finds the pool empty.
	releaseNoteTexture();
}

void Hint::onRemove()
{
	Object::onRemove();
	releaseNoteTexture();
	fadeScrollSound();
}

void Hint::fadeScrollSound()
{
	if(!p_scrollSound) return;

	// To zero, not to a negative target, which would pause it at the end: a
	// paused instance never reaches AL_STOPPED, so nothing reaps it and it
	// holds an audio source for the rest of the level. At zero it plays out
	// inaudibly and is reaped as usual.
	if(Sound::isLiveInstance(p_scrollSound)) p_scrollSound->slideVolume(0.0f, SCROLL_FADE_SPEED);
	p_scrollSound = 0;
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
	sprites.add(Vec2i(96, 288));
}

void Hint::bakeNote(const std::string& inLanguage)
{
	Engine& engine = Engine::inst();

	const std::string localized = inLanguage.empty() ? engine.localizeString(text) : engine.localizeString(text, inLanguage);
	const std::string wanted = p_font->adjustText(localized, TEXT_WIDTH);
	if(noteTexture && wanted == bakedText) return;

	// A texture of its own: stepping from one note to the next shows both,
	// and one must not bake into the sheet the other is drawn from.
	const Vec2i size(NOTE_TEXTURE_W, NOTE_TEXTURE_H);
	const uint target = noteTexture ? noteTexture : engine.acquireOffscreenTexture(size);
	if(!target) return;

	if(!engine.beginRenderToTexture(target, size))
	{
		if(!noteTexture) engine.releaseOffscreenTexture(target);
		return;
	}

	Renderer::inst().clear(Vec4f(0.0f, 0.0f, 0.0f, 0.0f));

	// The texture is blended again when drawn, so its alpha must be right:
	// the colour arrives weighted by source alpha, the alpha unweighted. The
	// result is premultiplied, which is how renderNote() draws it.
	Renderer::inst().setBlend(BM_BAKE);

	Renderer::inst().setTexture(p_sprite->ref());
	engine.renderSprite(Vec2i(0, 0), Vec2i(0, 0), Vec2i(NOTE_WIDTH, NOTE_HEIGHT), Vec4f(1.0f));

	// The same sheet again without the text: the back. Not mirrored, since
	// the paper curls about a horizontal axis and left stays left.
	engine.renderSprite(Vec2i(BACK_PANEL_X, 0), Vec2i(0, 0), Vec2i(NOTE_WIDTH, NOTE_HEIGHT), Vec4f(1.0f));

	p_font->renderText(wanted, Vec2i(TEXT_LEFT, TEXT_TOP), Vec4f(1.0f));

	Renderer::inst().setBlend(BM_NORMAL);
	engine.endRenderToTexture();

	noteTexture = target;
	bakedText = wanted;
}

void Hint::renderNoteMesh(const RenderState& state,
						  const Vec4f& color,
						  float unroll) const
{
	// Back to front, since nothing depth-tests: the bottom roll curls away,
	// so its outer end comes first, and the top roll curls forward, so its
	// outer end comes last. Each roll splits at the quarter turn into a front
	// and a back section, and the seam must fall on a vertex, or one band
	// would stretch its texture across both panels.
	const float uWidth = static_cast<float>(NOTE_WIDTH) / NOTE_TEXTURE_W;
	const float uBack = static_cast<float>(BACK_PANEL_X) / NOTE_TEXTURE_W;
	const float rolled = ROLL_LENGTH * NOTE_HEIGHT * (1.0f - unroll);
	const float flatTop = (rolled < 1.0f) ? 0.0f : rolled;
	const float flatBottom = NOTE_HEIGHT - flatTop;

	// Arc up to the quarter turn: radius * PI/2, i.e. flatTop / (4*ROLL_TURNS).
	// Where the roll does not reach that far, the back drops out as an empty
	// section - as it does for the flat sheet, where flatTop is 0 itself.
	const float half = min(flatTop, flatTop / (ROLL_TURNS * 4.0f));

	// [from, to] on the paper, how fine, and which panel of the texture.
	const int NUM_SECTIONS = 5;
	float sections[NUM_SECTIONS][2];
	int steps[NUM_SECTIONS];
	bool back[NUM_SECTIONS];
	sections[0][0] = NOTE_HEIGHT;         sections[0][1] = flatBottom + half; steps[0] = ROLL_BANDS / 2; back[0] = true;
	sections[1][0] = flatBottom + half;   sections[1][1] = flatBottom;        steps[1] = ROLL_BANDS / 2; back[1] = false;
	sections[2][0] = flatTop;             sections[2][1] = flatBottom;        steps[2] = 1;              back[2] = false;
	sections[3][0] = flatTop;             sections[3][1] = flatTop - half;    steps[3] = ROLL_BANDS / 2; back[3] = false;
	sections[4][0] = flatTop - half;      sections[4][1] = 0.0f;               steps[4] = ROLL_BANDS / 2; back[4] = true;

	// Two triangles a band, split as a triangle strip splits them, so every
	// diagonal runs the same way: a band is a trapezoid whose texture is not
	// affine across it, and the diagonal shows.
	Renderer& renderer = Renderer::inst();
	std::vector<Vertex> triangles;
	for(int section = 0; section < NUM_SECTIONS; section++)
	{
		const float from = sections[section][0];
		const float to = sections[section][1];
		if(from == to) continue;

		const float u0 = back[section] ? uBack : 0.0f;
		const float u1 = u0 + uWidth;

		Vertex left, right, previousLeft, previousRight;
		for(int k = 0; k <= steps[section]; k++)
		{
			const float py = from + (to - from) * k / steps[section];
			const NotePoint np = rollPoint(py, unroll);

			const float f = PERSPECTIVE / (PERSPECTIVE - np.depth);
			const float x = 0.5f * NOTE_WIDTH * f;
			const float y = np.y * f;
			const float dx = VIEW_OFFSET_X * (f - 1.0f);
			// Flipped: the bake drew with (0,0) at the top left, and a
			// texture starts at the bottom, so the note stands upside down in
			// it, as the game's frame does in the framebuffer object.
			const float t = 1.0f - py / NOTE_TEXTURE_H;

			// Premultiplied: the colour already carries the alpha in itself,
			// and the vertex colour we paint with has to take it along.
			const float b = np.shade * color.a;
			const Vec4f shaded(color.r * b, color.g * b, color.b * b, color.a);
			left.position = Vec2f(dx - x, y);
			left.uv = Vec2f(u0, t);
			left.color = shaded;
			right.position = Vec2f(dx + x, y);
			right.uv = Vec2f(u1, t);
			right.color = shaded;

			if(k)
			{
				triangles.push_back(previousLeft);
				triangles.push_back(previousRight);
				triangles.push_back(left);
				triangles.push_back(left);
				triangles.push_back(previousRight);
				triangles.push_back(right);
			}
			previousLeft = left;
			previousRight = right;
		}
	}
	if(!triangles.empty()) renderer.triangles(state, &triangles[0], static_cast<uint>(triangles.size()));
}

void Hint::renderNote(const Vec4f& color,
					  float unroll) const
{
	// The identity and not a texel scale: the mesh samples a fraction of
	// its own baked sheet rather than a count of texels. Blended
	// premultiplied, because the texture came about that way.
	const RenderState state(TextureRef(noteTexture, Vec2f(1.0f, 1.0f)), BM_PREMULTIPLIED);
	Renderer& renderer = Renderer::inst();

	// The shadow is the same paper in black, offset a little way.
	renderer.push();
	renderer.translate(SHADOW_OFFSET, SHADOW_OFFSET);
	renderNoteMesh(state, Vec4f(0.0f, 0.0f, 0.0f, color.a * SHADOW_ALPHA), unroll);
	renderer.pop();

	renderNoteMesh(state, color, unroll);
}

void Hint::onRender(RenderLayer layer,
					const Vec4f& color)
{
	if(layer == RL_MAIN) Engine::inst().renderSprites(sprites, color);
	else if(layer == RL_OVERLAY || layer == RL_HINT_PREVIEW)
	{
		float r = (0.85f - shownAlpha) * 45.0f;
		float i = shownAlpha / 0.85f;
		float s = i;
		float a = clamp(i / FADE_UNTIL, 0.0f, 1.0f);

		// RL_HINT_PREVIEW is the level editor's preview: opaque, unrolled and
		// centred, without touching targetPosition, which the game still uses.
		//
		// Only paper rolls (Level::isHintScroll()). A panel, as the space skin
		// shows, is drawn at 1.0, where both rolls drop out of the mesh.
		Vec2i target = targetPosition;
		float shownUnroll = level.isHintScroll() ? unroll : 1.0f;
		if(layer == RL_HINT_PREVIEW) a = 1.0f, r = 0.0f, i = 1.0f, s = 1.0f, target = Vec2i(320, 200), shownUnroll = 1.0f;

		// shownAlpha only approaches 0.85, so scale, angle and position would
		// stay a fraction off for ever and GL_LINEAR would mix every texel of
		// the text out of two. Under SNAP_RESIDUAL they snap to exactly 1, 0
		// and targetPosition, a Vec2i; the mesh's corners are whole numbers.
		if(1.0f - i < SNAP_RESIDUAL) i = 1.0f, s = 1.0f, r = 0.0f;

		// a, and not shownAlpha: the editor forces a to 1 above and drives
		// shownAlpha not at all, since it never runs Level::update().
		if(a > 1.0f / 255.0f)
		{
			// Does nothing while the baked text is still the one wanted.
			bakeNote(layer == RL_HINT_PREVIEW ? previewLanguage : "");

			Renderer& renderer = Renderer::inst();
			renderer.push();
			Vec2f p = static_cast<Vec2f>(-getShownPositionInPixels());
			renderer.translate(p.x, p.y);

			Vec4f realColor(color.r, color.g, color.b, color.a * a);

			renderer.push();
			Vec2f sp = (1.0f - i) * static_cast<Vec2f>(getShownPositionInPixels()) + i * static_cast<Vec2f>(target);
			renderer.translate(sp.x, sp.y);
			renderer.scale(s, s);
			renderer.rotate(r);

			// Nothing where the bake failed - out of texture memory, a lost
			// context. It is tried again next frame and shows when one works.
			if(noteTexture) renderNote(realColor, shownUnroll);

			renderer.pop();
			renderer.pop();
		}
	}
}

void Hint::onUpdate()
{
	Object* p_obj = level.getFrontObjectAt(position);
	const bool playerIsHere = (p_obj == level.getActivePlayer());

	// Walking away ends a dismissal: the note opens again on the next visit.
	if(!playerIsHere) dismissed = false;
	const bool open = playerIsHere && !dismissed;

	// The target is decided once, in the tick the note opens: decided later,
	// it would jump to the other side of the screen as the player steps off
	// the field. Not in onCollect(), which runs only once the player stands
	// at the centre, when the note is already on its way.
	if(open && activeTicks == 0)
	{
		updateTargetPosition();

		// No pitch spread: it would draw from the level's generator and shift
		// every random number after it.
		Engine::inst().playSound("hint.ogg", false, 0.0f, 100);
	}

	// By the clock and not by shownAlpha, which never quite arrives and would
	// leave the note a little rolled up for ever. A closed note rolls up
	// first, at the speed it opened at, where there is paper to roll; a panel
	// has nothing to roll and goes the moment it closes.
	if(open) { if(activeTicks < UNROLL_END) activeTicks++; }
	else if(level.isHintScroll()) { activeTicks = max(0, activeTicks - ROLL_UP_SPEED); }
	else { activeTicks = 0; }
	const float before = unroll;
	unroll = clamp(static_cast<float>(activeTicks - UNROLL_START) /
				   (UNROLL_END - UNROLL_START), 0.0f, 1.0f);

	// A rustle each time the paper sets off, unrolling or rolling up. A
	// motion cut short (closed while unrolling, reopened while rolling up)
	// fades its rustle quickly; one that runs to its end lets it play out.
	// Only paper rustles: a panel (no hintscroll.txt) is drawn flat.
	const int direction = unroll > before ? 1 : unroll < before ? -1 : 0;
	if(direction != scrollDirection)
	{
		if(direction != 0)
		{
			fadeScrollSound();
			if(level.isHintScroll()) p_scrollSound = Engine::inst().playSound("hintscroll.ogg", false, 0.0f, 100);
		}
		scrollDirection = direction;
	}

	// Roll up first, then disappear: while anything is left to roll up, the
	// note stays fully visible and in place.
	alpha = (open || unroll > 0.0f) ? 0.85f : 0.0f;
	shownAlpha = 0.15f * alpha + 0.85f * shownAlpha;
	if(shownAlpha <= 1.0f / 255.0f)
	{
		shownAlpha = 0.0f;
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
	// Deliberately empty: Object::onCollect() would make the note disappear,
	// and it stays to be read again.
}

bool Hint::dismiss()
{
	// Only when the note is showing something; otherwise Escape goes on to
	// the game menu. activeTicks rather than shownAlpha, which is still
	// fading out after a visit, when there is nothing to close.
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

void Hint::setPreviewLanguage(const std::string& language)
{
	previewLanguage = language;
}

void Hint::setText(const std::string& text)
{
	this->text = text;
}
