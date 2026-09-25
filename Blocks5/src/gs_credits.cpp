#include "pch.h"
#include "gs_credits.h"
#include "engine.h"
#include "font.h"
#include "texture.h"
#include "level.h"
#include "cf_all.h"
#include "campaign.h"

namespace
{
	// The blocks in the order they appear, in seconds of the full version:
	// start is when a block begins to fade in, duration how long until it has
	// gone again. Consecutive blocks overlap on purpose. ending marks the two
	// that address a player who has just won; the version the menu runs
	// before the shipped campaign is finished leaves them out, since they
	// would give away that there is an ending to reach.
	struct CreditsBlock
	{
		int x;
		int y;
		const char* p_title;
		const char* p_text;
		float start;
		float duration;
		float lineSpacing;
		bool ending;
	};

	const CreditsBlock p_blocks[] =
	{
		{0, 0, "", "$C_THANKS_FOR_PLAYING", 2.0f, 5.0f, 1.0f, true},
		{100, 100, "$C_PROGRAMMING", "David Scherfgen", 6.0f, 5.0f, 1.0f, false},
		{-100, -100, "$C_GRAPHICS", "David Scherfgen\nPatrick Jerusalem\nin2ear Productions", 10.0f, 7.0f, 1.0f, false},
		{-100, 100, "$C_SOUND_EFFECTS", "David Scherfgen\nin2ear Productions\nTobias Roesener", 16.0f, 7.0f, 1.0f, false},
		{100, -100, "$C_MUSIC", "in2ear Productions\nPatrick Jerusalem\nDavid Scherfgen", 22.0f, 7.0f, 1.0f, false},
		{-150, -150, "$C_TESTERS", "Tobias Roesener\nPatrick Jerusalem\nRolf Scherfgen\nGertrud Scherfgen\nFelix Scherfgen\nEckehard Kirmas\nIngmar Baum\nMartin Linnartz\nJan Hapke\nWilhelm Mail\xE4nder\nDennis Kleine-Arndt\nGuido Kie\xDFling\nChristian Ewald\nAlbert Kalchmair\nBernhard Kalchmair", 28.0f, 10.0f, 0.75f, false},
		{75, -200, "$C_TESTERS_SPPRO", "Abrexxes\nAnf\xE4nger\nbabelfish\nbig_muff\nBlack-Panther\nChase\nCodingCat\nDas Gurke\nDragonFlame\nFOGX\nFred\ngrek40\nHelmut\nkaid\nLemming\nmatthias\nPaul_C.\nRiddick\nSteveKr\nThomasS", 31.0f, 10.0f, 0.75f, false},
		{0, 0, "", "$C_STAY_TUNED", 45.0f, 8.0f, 1.0f, true}
	};
	const int NUM_BLOCKS = sizeof(p_blocks) / sizeof(p_blocks[0]);

	// A colour per block, so two shown together differ by more than their
	// position. Indexed by the block's place in the table, not among those
	// shown, so the short version keeps the same colours.
	const Vec4f p_blockColors[] = {Vec4f(1.0f, 1.0f, 1.0f, 1.0f),
								   Vec4f(1.0f, 0.9f, 0.9f, 1.0f),
								   Vec4f(0.9f, 1.0f, 0.9f, 1.0f),
								   Vec4f(0.9f, 0.9f, 1.0f, 1.0f),
								   Vec4f(1.0f, 1.0f, 0.9f, 1.0f),
								   Vec4f(0.9f, 1.0f, 1.0f, 1.0f),
								   Vec4f(1.0f, 0.9f, 1.0f, 1.0f)};
	const int NUM_BLOCK_COLORS = sizeof(p_blockColors) / sizeof(p_blockColors[0]);
}

GS_Credits::GS_Credits() : GameState("GS_Credits"), engine(Engine::inst())
{
	p_font = 0;
	p_level = 0;
	p_sprites = 0;
	leaving = false;
}

GS_Credits::~GS_Credits()
{
}

void GS_Credits::onRender()
{
	Renderer& renderer = Renderer::inst();
	renderer.setBlend(BM_NORMAL);
	float t = 0.001f * time;

	{
		// The colour channels only, up to the darkness at the end: the alpha
		// of the frame is left as it is.
		Renderer::ColorMaskScope colorOnly(true, true, true, false);

		// The frame is not cleared between frames (the ending covers it with
		// its gradient), so the plain version lays down its own black.
		if(full) renderStarField(t);
		else renderer.rect(Vec2f(0.0f, 0.0f), Vec2f(640.0f, 480.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f));

		for(int i = 0; i < NUM_BLOCKS; i++)
		{
			if(p_blocks[i].ending && !full) continue;

			const float blockStart = p_blocks[i].start - shift;
			if(t > blockStart && t < blockStart + p_blocks[i].duration)
			{
				std::string title = localizeString(p_blocks[i].p_title);
				std::string text = localizeString(p_blocks[i].p_text);

				Vec2i titleSize;
				Vec2i textSize;
				p_font->measureText(title, &titleSize, 0);
				p_font->measureText(text, &textSize, 0);

				// A block's life runs 0 to 2: in over the first half, out over
				// the second, the remainder squared so it leaves quickly and
				// then slowly. The ending also scales the letters, 0.75 to 1
				// on the way in and on to 3 on the way out, drifting up and
				// left so the growth reads as coming at the viewer. The plain
				// version does neither: on black, the zoom would be the only
				// thing moving.
				float alpha = 2.0f * (t - blockStart) / p_blocks[i].duration;
				float scaling = full ? 0.75f + 0.25f * alpha : 1.0f;
				Vec2f offset(0.0f, 0.0f);
				if(alpha > 1.0f)
				{
					if(full)
					{
						scaling = 1.0f + 2.0f * (alpha - 1.0f);
						offset -= Vec2f(6.0f, 8.0f) * (scaling - 1.0f);
					}
					alpha = 2.0f - alpha;
					alpha *= alpha;
				}

				renderer.push();
				renderer.translate(320.0f + offset.x, 225.0f + offset.y);
				renderer.translate(static_cast<float>(p_blocks[i].x), static_cast<float>(p_blocks[i].y));
				renderer.translate(static_cast<float>(titleSize.x / -2), 0.0f);

				Font::Options options = p_font->getOptions();
				options.shadows = 0;
				options.charSpacing = 2;
				options.charScaling = scaling;
				options.lineSpacing = p_blocks[i].lineSpacing;
				p_font->setOptions(options);
				// Cached only in the plain version: the ending animates
				// charScaling, which is part of the cache key, so no frame
				// would ever hit.
				p_font->renderText(title, Vec2i(0, 0), Vec4f(0.75f, 0.75f, 1.0f, alpha), !full);

				renderer.pop();
				renderer.push();

				renderer.translate(320.0f + offset.x, 255.0f + offset.y);
				renderer.translate(static_cast<float>(p_blocks[i].x), static_cast<float>(p_blocks[i].y));
				renderer.translate(static_cast<float>(textSize.x / -2), 0.0f);

				// Cached on the same terms as the title.
				p_font->renderText(text, Vec2i(0, 0), p_blockColors[i % NUM_BLOCK_COLORS] * Vec4f(1.0f, 1.0f, 1.0f, alpha), !full);

				renderer.pop();
			}
		}
	}

	float darkness = 0.0f;
	if(t < 0.0f) darkness = -0.5f * t;
	else if(t > fadeAt) darkness = 0.5f * (t - fadeAt);
	if(darkness > 0.0f)
	{
		renderer.rect(Vec2f(0.0f, 0.0f), Vec2f(640.0f, 480.0f), Vec4f(0.0f, 0.0f, 0.0f, darkness));
	}
}

void GS_Credits::onUpdate()
{
	// The clock the frame oracle freezes on, reported as Level::update and
	// GS_Loading report theirs. Shifted by the two seconds of lead-in so that
	// it never goes negative.
	engine.sceneTick = static_cast<uint>(time + 2000);

	// Only the ending flies through the star field.
	if(full)
	{
		cameraPos += 0.02f * 50.0f * cameraDir * static_cast<float>(speed);

		cameraDir += Vec3f(random(-0.002f, 0.002f), random(-0.002f, 0.002f), random(-0.002f, 0.002f));

		const float ct = 0.001f * time;
		cameraDir.x += 0.01f * sinf(ct * 0.1f);
		cameraDir.y += 0.01f * sinf(0.5f + ct * 0.05f);
		cameraDir.z += 0.01f * cosf(0.5f + ct * 0.075f);
		cameraDir.normalize();

		updateStars();
	}

	// The fast-forward is let go a second into the fade, a second before the
	// first goodbye, so that the end plays at its own speed whatever was done
	// before it.
	if(time >= static_cast<int>((fadeAt + 1.0f) * 1000.0f)) speed = 1;
	time += 20 * speed;

	// The three characters say goodbye into the black, two, three and four
	// seconds after the fade begins - in the ending only.
	if(full)
	{
		if(time == static_cast<int>((fadeAt + 2.0f) * 1000.0f)) engine.playSound("character1.ogg");
		if(time == static_cast<int>((fadeAt + 3.0f) * 1000.0f)) engine.playSound("character2.ogg");
		if(time == static_cast<int>((fadeAt + 4.0f) * 1000.0f)) engine.playSound("character3.ogg");
	}
	if(time >= static_cast<int>(endAt * 1000.0f)) leaveToMenu();

	// The four keys a player reaches for to be done with a screen. In the
	// ending they fast-forward, since it is there to be watched; in the plain
	// version, a screen offered from the menu, they end it, as does a click.
	const bool keyPressed = engine.wasKeyPressed(SDLK_RETURN) ||
							engine.wasKeyPressed(SDLK_KP_ENTER) ||
							engine.wasKeyPressed(SDLK_ESCAPE) ||
							engine.wasKeyPressed(SDLK_SPACE);

	if(full)
	{
		if(keyPressed)
		{
			speed = 5;
			time /= 100;
			time *= 100;
		}
	}
	else
	{
		// Never in the tick the screen was entered in: the click or Return
		// that opened it, onEnter and this first onUpdate all fall in one
		// tick, whose press bits are cleared only at its end.
		if(exitArmed && (keyPressed || engine.wasAnyButtonPressed())) leaveToMenu();
		exitArmed = true;
	}
}

void GS_Credits::leaveToMenu()
{
	// Once only: the clock and a click can both ask in one tick, and a second
	// setGameState() before the first is carried out would take the menu,
	// never entered, for the state being left - it would lose the focus and
	// leave on a title level it does not have.
	if(leaving) return;
	leaving = true;

	// The star wipe the menu goes anywhere else behind, on both ways out: a
	// screen that can be left at any moment is where a hard cut would show.
	engine.setGameState("GS_Menu");
	engine.crossfade(new CF_Star, 0.85f);
}

void GS_Credits::onEnter(const ParameterBlock& context)
{
	// Which version runs, said by the caller or worked out here. Both ways in
	// a player takes, the menu's Credits entry and the end of the shipped
	// campaign, leave it to this; after the campaign the level just finished
	// is already in the database. Only the author's keys (gs_menu.cpp) and
	// the frame oracle name a version outright.
	full = context.has("full") ? context.get<bool>("full") : Campaign::isBuiltInCompleted();

	// Where the first block shown starts, and everything after it with it.
	// The ending keeps its lead-in: two seconds of star field fading up from
	// black and two more before the thanks. On black, those four seconds
	// would look like a hang, so the plain version's clock starts at zero and
	// its first name fades up at half a second, inside the 0.85 s star wipe
	// it arrives behind, so that the two read as one change.
	time = full ? -2000 : 0;
	const float firstBlockAt = full ? p_blocks[0].start : 0.5f;

	shift = 0.0f;
	for(int i = 0; i < NUM_BLOCKS; i++)
	{
		if(p_blocks[i].ending && !full) continue;
		shift = p_blocks[i].start - firstBlockAt;
		break;
	}

	// The fade to black begins when the last block shown has gone. The state
	// hands back to the menu five seconds later in the ending, which is what
	// the three goodbyes need, and one second later in the plain version,
	// whose screen is black already.
	fadeAt = 0.0f;
	for(int i = 0; i < NUM_BLOCKS; i++)
	{
		if(p_blocks[i].ending && !full) continue;
		fadeAt = max(fadeAt, p_blocks[i].start + p_blocks[i].duration - shift);
	}
	endAt = fadeAt + (full ? 5.0f : 1.0f);

	speed = 1;
	exitArmed = false;
	leaving = false;
	p_font = Manager<Font>::inst().request("credits_font.xml");

	// The star field, its trail buffer and the level whose sprite sheet the
	// stars are cut from belong to the ending alone.
	p_level = 0;
	p_sprites = 0;
	bufferID = 0;
	if(full)
	{
		p_level = new Level;
		p_level->setInEditor(true);
		p_level->load("title.xml");
		p_sprites = p_level->getSpritesTexture();

		// create the texture for the effect buffer
		bufferID = engine.createFrameCopyTexture(false, true);
	}

	// Emptied, or a second visit would begin with the stars the first one
	// ended on.
	stars.clear();
	cameraPos = Vec3f(0.0f, 0.0f, 0.0f);
	cameraDir = Vec3f(0.0f, 0.0f, 1.0f);
}

void GS_Credits::onLeave(const ParameterBlock& context)
{
	p_font->release();
	p_font = 0;
	delete p_level;
	p_level = 0;
	p_sprites = 0;
	if(bufferID) Renderer::inst().deleteTexture(bufferID);
	bufferID = 0;
}

void GS_Credits::onGetFocus()
{
	// The ending hides the cursor; the plain version is a screen to click out
	// of and keeps it.
	if(full) SDL_ShowCursor(0);

	// The short version keeps the menu's track playing: the ending's would
	// announce an ending the player has not reached.
	if(full) engine.playMusic("credits.ogg", -1.0f);
}

void GS_Credits::onLoseFocus()
{
	SDL_ShowCursor(1);
}

void GS_Credits::renderStarField(float t)
{
	Renderer& renderer = Renderer::inst();

	Vec3f color(0.05f + 0.05f * sinf(t * 0.26f), 0.05f + 0.05f * cosf(t * 0.31f), 0.05f + 0.05f * sinf(t * 0.413f));
	{
		// a gradient, darkest at the top and bottom and twice as light in
		// the middle
		const Vec4f edge(color.r, color.g, color.b, 1.0f);
		const Vec4f middle(color.r * 2.0f, color.g * 2.0f, color.b * 2.0f, 1.0f);
		const Vec2f upper[4] = {Vec2f(0.0f, 0.0f), Vec2f(640.0f, 0.0f), Vec2f(640.0f, 240.0f), Vec2f(0.0f, 240.0f)};
		const Vec4f upperColors[4] = {edge, edge, middle, middle};
		renderer.quad(upper, upperColors);
		const Vec2f lower[4] = {Vec2f(0.0f, 240.0f), Vec2f(640.0f, 240.0f), Vec2f(640.0f, 480.0f), Vec2f(0.0f, 480.0f)};
		const Vec4f lowerColors[4] = {middle, middle, edge, edge};
		renderer.quad(lower, lowerColors);
	}

	const Vec2i& screenSize = engine.getScreenSize();

	// The last frame's stars at three quarters, which is what makes the
	// trails; the scale puts the texture coordinates in pixels.
	const Vec2f s = static_cast<Vec2f>(screenSize);
	const Vec2f screen[4] = {Vec2f(0.0f, 0.0f), Vec2f(s.x, 0.0f), s, Vec2f(0.0f, s.y)};
	renderer.quad(RenderState(engine.getFrameCopyRef(bufferID), BM_NORMAL), screen, screen, Vec4f(1.0f, 1.0f, 1.0f, 0.75f));

	const Mat4 projection = Mat4::perspective(90.0f, 1.0f, 0.1f, 500.0f);
	Vec3f lookAt = cameraPos + cameraDir;
	const Mat4 view = Mat4::lookAt(cameraPos.x, cameraPos.y, cameraPos.z, lookAt.x, lookAt.y, lookAt.z, 0.0f, 1.0f, 0.0f);
	renderStars(projection, view);

	// Before the text and after the stars: what is copied is what the next
	// frame draws back as trails, and the names are not meant to smear.
	engine.captureFrame(bufferID);
}

void GS_Credits::renderStars(const Mat4& projection,
							 const Mat4& view)
{
	// Every star in one draw. quads3D takes one matrix per call, so each
	// star's model transform, which is affine, is applied to its corners here
	// and projection * view rides on the draw. Measured under llvmpipe over
	// 500 frames: 4 draw calls a frame against 403 at one draw a star, and a
	// median render of 2.44 ms against 3.39. The stars blend in the list's
	// order, which updateStars() leaves back to front.
	const RenderState state(p_sprites->ref(), BM_NORMAL);

	starVertices.clear();
	for(std::list<Star>::const_iterator i = stars.begin(); i != stars.end(); ++i)
	{
		Mat4 model = Mat4::identity();
		model.translate(i->position.x, i->position.y, i->position.z);
		model.scale(i->size, i->size, i->size);
		model.rotate(i->rotation.x, 1.0f, 0.0f, 0.0f);
		model.rotate(i->rotation.y, 0.0f, 1.0f, 0.0f);
		model.rotate(i->rotation.z, 0.0f, 0.0f, 1.0f);

		float distSq = (i->position - cameraPos).lengthSq();
		float alpha = 1.0f / (1.0f + 0.001f * distSq);

		const Vec2f t = static_cast<Vec2f>(i->positionOnTexture);
		Vertex3 vertices[4];
		vertices[0].position = model.transformPoint3D(Vec3f(-0.5f, 0.5f, 0.0f));  vertices[0].uv = t;
		vertices[1].position = model.transformPoint3D(Vec3f(0.5f, 0.5f, 0.0f));   vertices[1].uv = t + Vec2f(16.0f, 0.0f);
		vertices[2].position = model.transformPoint3D(Vec3f(0.5f, -0.5f, 0.0f));  vertices[2].uv = t + Vec2f(16.0f, 16.0f);
		vertices[3].position = model.transformPoint3D(Vec3f(-0.5f, -0.5f, 0.0f)); vertices[3].uv = t + Vec2f(0.0f, 16.0f);
		for(int k = 0; k < 4; k++) vertices[k].color = Vec4f(1.0f, 1.0f, 1.0f, alpha);
		starVertices.insert(starVertices.end(), vertices, vertices + 4);
	}

	if(starVertices.empty()) return;
	Renderer::inst().quads3D(state, projection * view, &starVertices[0],
							 static_cast<uint>(starVertices.size()), false);
}

void GS_Credits::updateStars()
{
	// remove the stars that are no longer visible
	for(std::list<Star>::iterator i = stars.begin(); i != stars.end();)
	{
		i->rotation += 0.02f * i->deltaRotation;

		Vec3f d = i->position - cameraPos;
		float dot = d ^ cameraDir;
		if(dot <= 0.0f) i = stars.erase(i);
		else i++;
	}

	// add new stars
	while(stars.size() < 400)
	{
		Star s;
		Vec3f n(random(-1.0f, 1.0f), random(-1.0f, 1.0f), random(-1.0f, 1.0f));
		float l = n.length();
		n /= l + 0.001f;
		s.position = cameraPos + random(time == 0 ? 0.0f : 150.0f, 200.0f) * cameraDir + random(10.0f, 80.0f) * n;
		s.size = random(1.0f, 2.0f);
		s.rotation = Vec3f(random(0.0f, 10.0f), random(0.0f, 10.0f), random(0.0f, 10.0f));
		s.deltaRotation = Vec3f(random(-100.0f, 100.0f), random(-100.0f, 100.0f), random(-100.0f, 100.0f));
		// random(min, max) is inclusive at both ends, so 22 picks one of the
		// sheet's 23 rows; a 24th, at y = 736, lies below its 720 pixels.
		s.positionOnTexture = 32 * Vec2i(random(0, 7), random(0, 22));
		stars.push_back(s);
	}

	// Back to front by distance from the eye, the quantity the alpha fades
	// on: nothing depth-tests, so the order the blended quads are handed over
	// in decides which of two overlapping stars looks nearer. Birth order is
	// not that order - a star is born 70 to 280 away, while the camera gains
	// one unit a tick. Sorted here, once a tick, rather than in renderStars(),
	// which would sort every frame and write to the list it draws.
	struct
	{
		Vec3f eye;
		bool operator () (const Star& s1, const Star& s2) const
		{
			return (s1.position - eye).lengthSq() > (s2.position - eye).lengthSq();
		}
	} cmp;

	cmp.eye = cameraPos;
	stars.sort(cmp);
}