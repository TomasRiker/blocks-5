#include "pch.h"
#include "gs_credits.h"
#include "engine.h"
#include "font.h"
#include "texture.h"
#include "level.h"
#include "cf_all.h"

GS_Credits::GS_Credits() : GameState("GS_Credits"), engine(Engine::inst())
{
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

		Vec3f color(0.05f + 0.05f * sin(t * 0.26f), 0.05f + 0.05f * cos(t * 0.31f), 0.05f + 0.05f * sin(t * 0.413f));
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

		engine.captureFrame(bufferID);

		Vec4f textColors[] = {Vec4f(1.0f, 1.0f, 1.0f, 1.0f),
							  Vec4f(1.0f, 0.9f, 0.9f, 1.0f),
							  Vec4f(0.9f, 1.0f, 0.9f, 1.0f),
							  Vec4f(0.9f, 0.9f, 1.0f, 1.0f),
							  Vec4f(1.0f, 1.0f, 0.9f, 1.0f),
							  Vec4f(0.9f, 1.0f, 1.0f, 1.0f),
							  Vec4f(1.0f, 0.9f, 1.0f, 1.0f)};

		struct
		{
			Vec2i position;
			std::string title;
			std::string text;
			float start;
			float duration;
		} texts[] = {

		Vec2i(0, 0),
		"",
		"$C_THANKS_FOR_PLAYING",
		2.0f,
		5.0f,

		Vec2i(100, 100),
		"$C_PROGRAMMING",
		"David Scherfgen",
		6.0f,
		5.0f,

		Vec2i(-100, -100),
		"$C_GRAPHICS",
		"David Scherfgen\nPatrick Jerusalem\nin2ear Productions",
		10.0f,
		7.0f,

		Vec2i(-100, 100),
		"$C_SOUND_EFFECTS",
		"David Scherfgen\nin2ear Productions\nTobias Roesener",
		16.0f,
		7.0f,

		Vec2i(100, -100),
		"$C_MUSIC",
		"in2ear Productions\nPatrick Jerusalem\nDavid Scherfgen",
		22.0f,
		7.0f,

		Vec2i(-150, -150),
		"$C_TESTERS",
		"Tobias Roesener\nPatrick Jerusalem\nRolf Scherfgen\nGertrud Scherfgen\nFelix Scherfgen\nEckehard Kirmas\nIngmar Baum\nMartin Linnartz\nJan Hapke\nWilhelm Mail\xE4nder\nDennis Kleine-Arndt\nGuido Kie\xDFling\nChristian Ewald\nAlbert Kalchmair\nBernhard Kalchmair",
		28.0f,
		10.0f,

		Vec2i(75, -200),
		"$C_TESTERS_SPPRO",
		"Abrexxes\nAnf\xE4nger\nbabelfish\nbig_muff\nBlack-Panther\nChase\nCodingCat\nDas Gurke\nDragonFlame\nFOGX\nFred\ngrek40\nHelmut\nkaid\nLemming\nmatthias\nPaul_C.\nRiddick\nSteveKr\nThomasS",
		31.0f,
		10.0f,

		Vec2i(0, 0),
		"",
		"$C_STAY_TUNED",
		45.0f,
		8.0f
		};

		for(int i = 0; i < sizeof(texts) / sizeof(texts[0]); i++)
		{
			if(t > texts[i].start && t < texts[i].start + texts[i].duration)
			{
				std::string title = localizeString(texts[i].title);
				std::string text = localizeString(texts[i].text);

				Vec2i titleSize;
				Vec2i textSize;
				p_font->measureText(title, &titleSize, 0);
				p_font->measureText(text, &textSize, 0);

				float alpha = 2.0f * (t - texts[i].start) / texts[i].duration;
				float scaling = 0.75f + 0.25f * alpha;
				Vec2f offset(0.0f, 0.0f);
				if(alpha > 1.0f)
				{
					scaling = 1.0f + 2.0f * (alpha - 1.0f);
					alpha = 2.0f - alpha;
					alpha *= alpha;
					offset -= Vec2f(6.0f, 8.0f) * (scaling - 1.0f);
				}

				renderer.push();
				renderer.translate(320.0f + offset.x, 225.0f + offset.y);
				renderer.translate(static_cast<float>(texts[i].position.x), static_cast<float>(texts[i].position.y));
				renderer.translate(static_cast<float>(titleSize.x / -2), 0.0f);

				Font::Options options = p_font->getOptions();
				options.shadows = 0;
				options.charSpacing = 2;
				options.charScaling = scaling;
				options.lineSpacing = (i == 5 || i == 6) ? 0.75f : 1.0f;
				p_font->setOptions(options);
				// Not cached: charScaling is animated, so this key belongs to this
				// frame and to no other.
				p_font->renderText(title, Vec2i(0, 0), Vec4f(0.75f, 0.75f, 1.0f, alpha), false);

				renderer.pop();
				renderer.push();

				renderer.translate(320.0f + offset.x, 255.0f + offset.y);
				renderer.translate(static_cast<float>(texts[i].position.x), static_cast<float>(texts[i].position.y));
				renderer.translate(static_cast<float>(textSize.x / -2), 0.0f);

				// Uncached for the same reason as the title above: scaling is
				// 0.75 + 0.25 * alpha and both draws are laid out under it.
				p_font->renderText(text, Vec2i(0, 0), textColors[i % (sizeof(textColors) / sizeof(textColors[0]))] * Vec4f(1.0f, 1.0f, 1.0f, alpha), false);

				renderer.pop();
			}
		}
	}

	float darkness = 0.0f;
	if(t < 0.0f) darkness = -0.5f * t;
	else if(t > 53.0f) darkness = 0.5f * (t - 53.0f);
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

	cameraPos += 0.02f * 50.0f * cameraDir * static_cast<float>(speed);

	cameraDir += Vec3f(random(-0.002f, 0.002f), random(-0.002f, 0.002f), random(-0.002f, 0.002f));

	float t = 0.001f * time;
	cameraDir.x += 0.01f * sin(t * 0.1f);
	cameraDir.y += 0.01f * sin(0.5f + t * 0.05f);
	cameraDir.z += 0.01f * cos(0.5f + t * 0.075f);
	cameraDir.normalize();

	updateStars();

	if(time >= 54 * 1000) speed = 1;
	time += 20 * speed;

	if(time == 55 * 1000) engine.playSound("character1.ogg");
	if(time == 56 * 1000) engine.playSound("character2.ogg");
	if(time == 57 * 1000) engine.playSound("character3.ogg");
	if(time == 58 * 1000) engine.setGameState("GS_Menu");

	if(engine.wasKeyPressed(SDLK_RETURN) ||
	   engine.wasKeyPressed(SDLK_KP_ENTER) ||
	   engine.wasKeyPressed(SDLK_ESCAPE) ||
	   engine.wasKeyPressed(SDLK_SPACE))
	{
		speed = 5;
		time /= 100;
		time *= 100;
	}
}

void GS_Credits::onEnter(const ParameterBlock& context)
{
	time = -2000;
	speed = 1;
	p_font = Manager<Font>::inst().request("credits_font.xml");
	p_level = new Level;
	p_level->setInEditor(true);
	p_level->load("title.xml");
	p_sprites = p_level->getSpritesTexture();

	// create the texture for the effect buffer
	bufferID = engine.createFrameCopyTexture(false, true);

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
	Renderer::inst().deleteTexture(bufferID);
	bufferID = 0;
}

void GS_Credits::onGetFocus()
{
	SDL_ShowCursor(0);
	engine.playMusic("credits.ogg", -1.0f);
}

void GS_Credits::onLoseFocus()
{
	SDL_ShowCursor(1);
}

void GS_Credits::renderStars(const Mat4& projection,
							 const Mat4& view)
{
	// Each star a unit quad under a matrix of its own, one draw each, in
	// the sprite sheet's texels.
	const RenderState state(p_sprites->ref(), BM_NORMAL);

	for(std::list<Star>::reverse_iterator i = stars.rbegin(); i != stars.rend(); ++i)
	{
		Mat4 modelview = view;
		modelview.translate(i->position.x, i->position.y, i->position.z);
		modelview.scale(i->size, i->size, i->size);
		modelview.rotate(i->rotation.x, 1.0f, 0.0f, 0.0f);
		modelview.rotate(i->rotation.y, 0.0f, 1.0f, 0.0f);
		modelview.rotate(i->rotation.z, 0.0f, 0.0f, 1.0f);

		float distSq = (i->position - cameraPos).lengthSq();
		float alpha = 1.0f / (1.0f + 0.001f * distSq);

		const Vec2f t = static_cast<Vec2f>(i->positionOnTexture);
		Vertex3 vertices[4];
		vertices[0].position = Vec3f(-0.5f, 0.5f, 0.0f);  vertices[0].uv = t;
		vertices[1].position = Vec3f(0.5f, 0.5f, 0.0f);   vertices[1].uv = t + Vec2f(16.0f, 0.0f);
		vertices[2].position = Vec3f(0.5f, -0.5f, 0.0f);  vertices[2].uv = t + Vec2f(16.0f, 16.0f);
		vertices[3].position = Vec3f(-0.5f, -0.5f, 0.0f); vertices[3].uv = t + Vec2f(0.0f, 16.0f);
		for(int k = 0; k < 4; k++) vertices[k].color = Vec4f(1.0f, 1.0f, 1.0f, alpha);
		Renderer::inst().quads3D(state, projection * modelview, vertices, 4, false);
	}
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
		s.positionOnTexture = 32 * Vec2i(random(0, 7), random(0, 23));
		stars.push_back(s);
	}
}