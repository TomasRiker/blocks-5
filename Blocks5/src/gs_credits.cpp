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
	double t = 0.001 * time;

	{
		// The colour channels only, up to the darkness at the end: the alpha
		// of the frame is left as it is.
		Renderer::ColorMaskScope colorOnly(true, true, true, false);

		Vec3d color(0.05 + 0.05 * sin(t * 0.26), 0.05 + 0.05 * cos(t * 0.31), 0.05 + 0.05 * sin(t * 0.413));
		{
			// a gradient, darkest at the top and bottom and twice as light in
			// the middle
			const Vec4f edge(static_cast<float>(color.r), static_cast<float>(color.g), static_cast<float>(color.b), 1.0f);
			const Vec4f middle(static_cast<float>(color.r * 2.0), static_cast<float>(color.g * 2.0), static_cast<float>(color.b * 2.0), 1.0f);
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
		const Vec2f screen[4] = {Vec2f(0.0f, 0.0f), Vec2f(screenSize.x, 0.0f), Vec2f(screenSize.x, screenSize.y), Vec2f(0.0f, screenSize.y)};
		renderer.quad(RenderState(engine.getFrameCopyRef(bufferID), BM_NORMAL), screen, screen, Vec4f(1.0f, 1.0f, 1.0f, 0.75f));

		const Mat4 projection = Mat4::perspective(90.0, 1.0, 0.1, 500.0);
		Vec3d lookAt = cameraPos + cameraDir;
		const Mat4 view = Mat4::lookAt(cameraPos.x, cameraPos.y, cameraPos.z, lookAt.x, lookAt.y, lookAt.z, 0.0, 1.0, 0.0);
		renderStars(projection, view);

		engine.captureFrame(bufferID);

		Vec4d textColors[] = {Vec4d(1.0, 1.0, 1.0, 1.0),
							  Vec4d(1.0, 0.9, 0.9, 1.0),
							  Vec4d(0.9, 1.0, 0.9, 1.0),
							  Vec4d(0.9, 0.9, 1.0, 1.0),
							  Vec4d(1.0, 1.0, 0.9, 1.0),
							  Vec4d(0.9, 1.0, 1.0, 1.0),
							  Vec4d(1.0, 0.9, 1.0, 1.0)};

		struct
		{
			Vec2i position;
			std::string title;
			std::string text;
			double start;
			double duration;
		} texts[] = {

		Vec2i(0, 0),
		"",
		"$C_THANKS_FOR_PLAYING",
		2.0,
		5.0,

		Vec2i(100, 100),
		"$C_PROGRAMMING",
		"David Scherfgen",
		6.0,
		5.0,

		Vec2i(-100, -100),
		"$C_GRAPHICS",
		"David Scherfgen\nPatrick Jerusalem\nin2ear Productions",
		10.0,
		7.0,

		Vec2i(-100, 100),
		"$C_SOUND_EFFECTS",
		"David Scherfgen\nin2ear Productions\nTobias Roesener",
		16.0,
		7.0,

		Vec2i(100, -100),
		"$C_MUSIC",
		"in2ear Productions\nPatrick Jerusalem\nDavid Scherfgen",
		22.0,
		7.0,

		Vec2i(-150, -150),
		"$C_TESTERS",
		"Tobias Roesener\nPatrick Jerusalem\nRolf Scherfgen\nGertrud Scherfgen\nFelix Scherfgen\nEckehard Kirmas\nIngmar Baum\nMartin Linnartz\nJan Hapke\nWilhelm Mail\xE4nder\nDennis Kleine-Arndt\nGuido Kie\xDFling\nChristian Ewald\nAlbert Kalchmair\nBernhard Kalchmair",
		28.0,
		10.0,

		Vec2i(75, -200),
		"$C_TESTERS_SPPRO",
		"Abrexxes\nAnf\xE4nger\nbabelfish\nbig_muff\nBlack-Panther\nChase\nCodingCat\nDas Gurke\nDragonFlame\nFOGX\nFred\ngrek40\nHelmut\nkaid\nLemming\nmatthias\nPaul_C.\nRiddick\nSteveKr\nThomasS",
		31.0,
		10.0,

		Vec2i(0, 0),
		"",
		"$C_STAY_TUNED",
		45.0,
		8.0
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

				double alpha = 2.0 * (t - texts[i].start) / texts[i].duration;
				double scaling = 0.75 + 0.25 * alpha;
				Vec2d offset(0.0, 0.0);
				if(alpha > 1.0)
				{
					scaling = 1.0 + 2.0 * (alpha - 1.0);
					alpha = 2.0 - alpha;
					alpha *= alpha;
					offset -= Vec2d(6.0, 8.0) * (scaling - 1.0);
				}

				renderer.push();
				renderer.translate(320.0 + offset.x, 225.0 + offset.y);
				renderer.translate(texts[i].position.x, texts[i].position.y);
				renderer.translate(titleSize.x / -2, 0.0);

				Font::Options options = p_font->getOptions();
				options.shadows = 0;
				options.charSpacing = 2;
				options.charScaling = scaling;
				options.lineSpacing = (i == 5 || i == 6) ? 0.75 : 1.0;
				p_font->setOptions(options);
				// Not cached: charScaling is animated, so this key belongs to this
				// frame and to no other.
				p_font->renderText(title, Vec2i(0, 0), Vec4d(0.75, 0.75, 1.0, alpha), false);

				renderer.pop();
				renderer.push();

				renderer.translate(320.0 + offset.x, 255.0 + offset.y);
				renderer.translate(texts[i].position.x, texts[i].position.y);
				renderer.translate(textSize.x / -2, 0.0);

				// Uncached for the same reason as the title above: scaling is
				// 0.75 + 0.25 * alpha and both draws are laid out under it.
				p_font->renderText(text, Vec2i(0, 0), textColors[i % (sizeof(textColors) / sizeof(textColors[0]))] * Vec4d(1.0, 1.0, 1.0, alpha), false);

				renderer.pop();
			}
		}
	}

	double darkness = 0.0;
	if(t < 0.0) darkness = -0.5 * t;
	else if(t > 53.0) darkness = 0.5 * (t - 53.0);
	if(darkness > 0.0)
	{
		renderer.rect(Vec2f(0.0f, 0.0f), Vec2f(640.0f, 480.0f), Vec4f(0.0f, 0.0f, 0.0f, static_cast<float>(darkness)));
	}
}

void GS_Credits::onUpdate()
{
	// The clock the frame oracle freezes on, as Level::update reports its
	// own: the credits are the one screen outside a level with a clock that
	// starts when the screen does. Shifted by the two seconds of lead-in so
	// that it never goes negative.
	engine.sceneTick = static_cast<uint>(time + 2000);

	cameraPos += 0.02 * 50.0 * cameraDir * speed;

	cameraDir += Vec3d(random(-0.002, 0.002), random(-0.002, 0.002), random(-0.002, 0.002));

	double t = 0.001 * time;
	cameraDir.x += 0.01 * sin(t * 0.1);
	cameraDir.y += 0.01 * sin(0.5 + t * 0.05);
	cameraDir.z += 0.01 * cos(0.5 + t * 0.075);
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

	cameraPos = Vec3d(0.0, 0.0, 0.0);
	cameraDir = Vec3d(0.0, 0.0, 1.0);
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
	engine.playMusic("credits.ogg", -1.0);
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
		modelview.rotate(i->rotation.x, 1.0, 0.0, 0.0);
		modelview.rotate(i->rotation.y, 0.0, 1.0, 0.0);
		modelview.rotate(i->rotation.z, 0.0, 0.0, 1.0);

		double distSq = (i->position - cameraPos).lengthSq();
		double alpha = 1.0 / (1.0 + 0.001 * distSq);

		const Vec2i& t = i->positionOnTexture;
		Vertex3 vertices[4];
		vertices[0].position = Vec3f(-0.5f, 0.5f, 0.0f);  vertices[0].uv = Vec2f(t.x, t.y);
		vertices[1].position = Vec3f(0.5f, 0.5f, 0.0f);   vertices[1].uv = Vec2f(t.x + 16, t.y);
		vertices[2].position = Vec3f(0.5f, -0.5f, 0.0f);  vertices[2].uv = Vec2f(t.x + 16, t.y + 16);
		vertices[3].position = Vec3f(-0.5f, -0.5f, 0.0f); vertices[3].uv = Vec2f(t.x, t.y + 16);
		for(int k = 0; k < 4; k++) vertices[k].color = Vec4f(1.0f, 1.0f, 1.0f, static_cast<float>(alpha));
		Renderer::inst().quads3D(state, projection * modelview, vertices, 4, false);
	}
}

void GS_Credits::updateStars()
{
	// remove the stars that are no longer visible
	for(std::list<Star>::iterator i = stars.begin(); i != stars.end();)
	{
		i->rotation += 0.02 * i->deltaRotation;

		Vec3d d = i->position - cameraPos;
		double dot = d ^ cameraDir;
		if(dot <= 0.0) i = stars.erase(i);
		else i++;
	}

	// add new stars
	while(stars.size() < 400)
	{
		Star s;
		Vec3d n(random(-1.0, 1.0), random(-1.0, 1.0), random(-1.0, 1.0));
		double l = n.length();
		n /= l + 0.001;
		s.position = cameraPos + random(time == 0 ? 0.0 : 150.0, 200.0) * cameraDir + random(10.0, 80.0) * n;
		s.size = random(1.0, 2.0);
		s.rotation = Vec3d(random(0.0, 10.0), random(0.0, 10.0), random(0.0, 10.0));
		s.deltaRotation = Vec3d(random(-100.0, 100.0), random(-100.0, 100.0), random(-100.0, 100.0));
		s.positionOnTexture = 32 * Vec2i(random(0, 7), random(0, 23));
		stars.push_back(s);
	}
}