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
	// The blocks the credits show, in the order they appear, with the seconds
	// of the full version: start is when a block begins to fade in, duration
	// how long it has before it has gone again, and consecutive blocks
	// deliberately overlap - the two tester lists stand side by side for most
	// of theirs.
	//
	// ending marks the two that address a player who has just won. They are
	// left out of the version the menu runs before the shipped campaign is
	// finished: "thanks for playing" is addressed to somebody who has, and
	// "stay tuned" gives away that there is an ending to reach at all.
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

	// A colour a block, so that two standing together are told apart by more
	// than their position. By the block's place in the table and not in what
	// is shown, or the short version would recolour everything it inherits.
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

		// The frame buffer is not cleared between frames - the ending covers
		// it with its gradient - so the plain version has to lay the black
		// down itself rather than leave what was there.
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

				// A block's life is one number running 0 to 2: in over the
				// first half, out over the second, where squaring the
				// remainder makes the leaving quick and then slow. The ending
				// also scales the letters with it - three quarters up to full
				// on the way in, then away to three times, drifting up and
				// left so that the growth reads as coming at the viewer. The
				// plain version has neither: it is text on black, where a
				// per-character zoom would be the only thing moving.
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
				// Cached only where the layout stands still. The ending
				// animates charScaling, which is part of the cache key, so
				// every frame builds a key no other will ask for; the plain
				// version holds it at 1 and every frame hits.
				p_font->renderText(title, Vec2i(0, 0), Vec4f(0.75f, 0.75f, 1.0f, alpha), !full);

				renderer.pop();
				renderer.push();

				renderer.translate(320.0f + offset.x, 255.0f + offset.y);
				renderer.translate(static_cast<float>(p_blocks[i].x), static_cast<float>(p_blocks[i].y));
				renderer.translate(static_cast<float>(textSize.x / -2), 0.0f);

				// Cached on the same terms as the title above: both draws are
				// laid out under the one scaling.
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
	// The clock the frame oracle freezes on, as Level::update reports its
	// own: the credits are the one screen outside a level with a clock that
	// starts when the screen does. Shifted by the two seconds of lead-in so
	// that it never goes negative.
	engine.sceneTick = static_cast<uint>(time + 2000);

	// The flight through the star field is the ending's, so nothing steers a
	// camera or keeps four hundred stars alive for a screen that is text on
	// black.
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

	// The fast-forward is let go of a second before the fade, so that the end
	// is watched at the speed it was written at whatever was done before it.
	if(time >= static_cast<int>((fadeAt + 1.0f) * 1000.0f)) speed = 1;
	time += 20 * speed;

	// The three characters say goodbye into the black, two, three and four
	// seconds after the fade begins. They belong to the ending and not to the
	// names, so the short version has nobody to say it and no reason to wait.
	if(full)
	{
		if(time == static_cast<int>((fadeAt + 2.0f) * 1000.0f)) engine.playSound("character1.ogg");
		if(time == static_cast<int>((fadeAt + 3.0f) * 1000.0f)) engine.playSound("character2.ogg");
		if(time == static_cast<int>((fadeAt + 4.0f) * 1000.0f)) engine.playSound("character3.ogg");
	}
	if(time >= static_cast<int>(endAt * 1000.0f)) leaveToMenu();

	// The four keys a player reaches for to be done with a screen. In the
	// ending they hurry it, because it is something to watch and cutting it
	// off is not what somebody who has just won is asking for; in the plain
	// version they end it, as does any click, because that one is a screen
	// offered from the menu - it keeps the cursor, and there is nothing to
	// fast-forward through when the way out is the point.
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
		// Never in the tick the screen was entered in: a menu entry answers
		// a click or a Return, GUI::update() dispatches it,
		// processGameStateChanges() runs onEnter and onUpdate follows, all
		// inside the tick whose press bits are cleared only at its foot - so
		// whatever opened the screen is still standing when it first asks.
		if(exitArmed && (keyPressed || engine.wasAnyButtonPressed())) leaveToMenu();
		exitArmed = true;
	}
}

void GS_Credits::leaveToMenu()
{
	// Once, whichever asks first: the clock and a click can land in the same
	// tick, and a second setGameState() before the first is carried out takes
	// the menu it pushed for the state being left - never entered, it would
	// lose the focus and leave on a title level it does not have.
	if(leaving) return;
	leaving = true;

	// The same star the menu goes anywhere else behind, and the one it came
	// in behind. Both ways out take it, the clock running out and the player
	// saying enough: the second is the one that would otherwise cut, and a
	// screen that can be left at any moment is exactly where a cut shows.
	engine.setGameState("GS_Menu");
	engine.crossfade(new CF_Star, 0.85f);
}

void GS_Credits::onEnter(const ParameterBlock& context)
{
	// Which of the two runs, said by the caller or worked out here. Nothing
	// has to say it: both ways in that a player takes - a Credits entry in
	// the menu, and the last level of the shipped campaign - leave it, and
	// the one after the campaign is right for free, because the level just
	// finished is already in the database. The keys that name a version
	// outright are the author's (gs_menu.cpp), and the frame oracle's.
	full = context.has("full") ? context.get<bool>("full") : Campaign::isBuiltInCompleted();

	// What the first block shown is moved to, and with it everything behind
	// it. The ending keeps its lead-in: two seconds of star field while the
	// screen fades up from black, and two more before the thanks, which is an
	// establishing shot. The plain version's are two seconds of black fading
	// up from black and two of black, which is four seconds nobody can tell
	// from a game that has hung - so its clock starts at zero rather than two
	// seconds before it, and half a second is all the lead-in it keeps: the
	// star wipe it arrives behind takes 0.85 s, and a name fading up under a
	// wipe that is still running reads as one thing happening rather than two.
	time = full ? -2000 : 0;
	const float firstBlockAt = full ? p_blocks[0].start : 0.5f;

	shift = 0.0f;
	for(int i = 0; i < NUM_BLOCKS; i++)
	{
		if(p_blocks[i].ending && !full) continue;
		shift = p_blocks[i].start - firstBlockAt;
		break;
	}

	// The fade to black begins when the last block shown has gone, and the
	// state hands back to the menu after it: five seconds in the ending,
	// which is what the three goodbyes need. The plain version's last name
	// has faded out on a screen that was already black, so the fade over it
	// is a fade from black to black - one second, a beat before the star
	// wipe, rather than two of watching nothing happen.
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

	// The star field and the buffer its trails come back out of belong to the
	// ending alone, and the level is loaded only because the stars are cut
	// from its sprite sheet - so the plain version loads none of the three.
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

	// Emptied rather than left: a second visit would otherwise begin with the
	// stars the first one ended on, standing wherever the camera left them.
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
	// The ending takes the screen and the pointer with it; the plain version
	// is a screen to click out of, so the cursor stays where it can be seen.
	if(full) SDL_ShowCursor(0);

	// The short version leaves the music alone: it is run from the menu, the
	// menu's own track is playing, and swapping it for the ending's would
	// announce an ending the player has not reached - and stop the menu music
	// dead on the way back, since playMusic() resumes a track it never left.
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
	// Every star in one draw. quads3D takes one matrix per call, so a star
	// that carried its own modelview into it was a flush, a buffer upload
	// and a glDrawElements of its own: 403 draw calls a frame at 1.1 quads
	// to a draw, where the busiest screen in the game otherwise asks for 29.
	// What is per star is the model transform alone, and it is affine, so
	// the four corners are put through it here - which is the arithmetic the
	// renderer already does to every 2D quad it bakes - and projection *
	// view, the same for all of them, rides on the draw. Measured over 500
	// frames of the running screen under llvmpipe: 403 draw calls a frame to
	// 4, and the median render 3.39 ms to 2.44. Read that as a floor - a
	// software rasterizer spends its time filling, where a real driver and a
	// phone pay most of it per call.
	//
	// The order is the list's, which updateStars() leaves sorted back to
	// front, and one draw keeps it: the index buffer runs straight through
	// the stream, so the stars blend in the order they were handed in exactly
	// as they did one draw each. What moves is the last bit: the corner is
	// rounded once by the model matrix and again by projection * view, where
	// a draw a star rounds it once by the product of all three - a difference
	// of 197 of the oracle frame's 307200 pixels, by at most 3 of 255.
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
		// Twenty-three rows of the sheet and not twenty-four: random(min, max)
		// is inclusive at both ends - MTRand::randInt(n) gives [0, n] - so a 23
		// here picked a cell at row 736, which is below the last row the art
		// reaches. One star in twenty-four drew nothing.
		s.positionOnTexture = 32 * Vec2i(random(0, 7), random(0, 22));
		stars.push_back(s);
	}

	// Back to front, because nothing in this game depth-tests: a star is an
	// alpha-blended quad, and the only thing deciding which of two overlapping
	// ones looks nearer is the order they are handed to the renderer in.
	//
	// The order they are born in is not that order and is not close to it. A
	// star appears 150 to 200 ahead of the camera with an offset of up to 80
	// in a random direction, so it is 70 to 280 away at birth - against the
	// one unit a tick the camera gains on it. Walking the list backwards,
	// newest first, put 52% of all pairs the wrong way round through the
	// lead-in, whose four hundred are all born in its first tick and so in no
	// order at all, and 21 to 27% once stars are being recycled; about half
	// of the adjacent pairs were wrong throughout. Sorted, none are.
	//
	// What it is worth is small: under BM_NORMAL two orders of a pair differ
	// by a1 * a2 * (S1 - S2) per pixel, and a star is faint until it has come
	// close - the oracle's frozen frame moves by 46 of 307200 pixels. The
	// sort costs one pass a tick.
	//
	// Here rather than in renderStars(), which would sort once a frame for an
	// order that can only change when the camera moves - and the camera moves
	// here. It also keeps the render path from writing to the list it draws.
	//
	// By distance from the eye, the same quantity the alpha fades on, so there
	// is one notion of far on this screen and not two.
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