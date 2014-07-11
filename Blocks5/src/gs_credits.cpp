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
	glColorMask(true, true, true, false);

	glDisable(GL_TEXTURE_2D);
	glBegin(GL_QUADS);
	double t = 0.001 * time;
	Vec3d color(0.05 + 0.05 * sin(t * 0.26), 0.05 + 0.05 * cos(t * 0.31), 0.05 + 0.05 * sin(t * 0.413));
	glColor3dv(color);
	glVertex2i(0, 0);
	glVertex2i(640, 0);
	glColor3dv(color * 2.0);
	glVertex2i(640, 240);
	glVertex2i(0, 240);
	glVertex2i(0, 240);
	glVertex2i(640, 240);
	glColor3dv(color);
	glVertex2i(640, 480);
	glVertex2i(0, 480);
	glEnd();

	const Vec2i& screenSize = Engine::inst().getScreenSize();
	const Vec2i& screenPow2Size = Engine::inst().getScreenPow2Size();

	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	double w = static_cast<double>(screenPow2Size.x), h = static_cast<double>(screenPow2Size.y);
	glScaled(1.0 / w, -1.0 / h, 1.0);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, bufferID);
	glBegin(GL_QUADS);
	glColor4d(1.0, 1.0, 1.0, 0.75);
	glTexCoord2i(0, 0);
	glVertex2i(0, 0);
	glTexCoord2i(screenSize.x, 0);
	glVertex2i(screenSize.x, 0);
	glTexCoord2i(screenSize.x, screenSize.y);
	glVertex2i(screenSize.x, screenSize.y);
	glTexCoord2i(0, screenSize.y);
	glVertex2i(0, screenSize.y);
	glEnd();

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	gluPerspective(90.0, 1.0, 0.1, 500.0);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	Vec3d lookAt = cameraPos + cameraDir;
	gluLookAt(cameraPos.x, cameraPos.y, cameraPos.z, lookAt.x, lookAt.y, lookAt.z, 0.0, 1.0, 0.0);

	renderStars();

	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);

	glBindTexture(GL_TEXTURE_2D, bufferID);
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, screenPow2Size.y - screenSize.y, 0, 0, screenSize.x, screenSize.y);

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
	"Tobias Roesener\nPatrick Jerusalem\nRolf Scherfgen\nGertrud Scherfgen\nFelix Scherfgen\nEckehard Kirmas\nIngmar Baum\nMartin Linnartz\nJan Hapke\nWilhelm Mail‰nder\nDennis Kleine-Arndt\nGuido Kieﬂling\nChristian Ewald\nAlbert Kalchmair\nBernhard Kalchmair",
	28.0,
	10.0,

	Vec2i(75, -200),
	"$C_TESTERS_SPPRO",
	"Abrexxes\nAnf‰nger\nbabelfish\nbig_muff\nBlack-Panther\nChase\nCodingCat\nDas Gurke\nDragonFlame\nFOGX\nFred\ngrek40\nHelmut\nkaid\nLemming\nmatthias\nPaul_C.\nRiddick\nSteveKr\nThomasS",
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

			glPushMatrix();
			glTranslated(320.0 + offset.x, 225.0 + offset.y, 0.0);
			glTranslated(texts[i].position.x, texts[i].position.y, 0.0);
			glTranslated(titleSize.x / -2, 0.0, 0.0);

			Font::Options options = p_font->getOptions();
			options.shadows = 0;
			options.charSpacing = 2;
			options.charScaling = scaling;
			options.lineSpacing = (i == 5 || i == 6) ? 0.75 : 1.0;
			p_font->setOptions(options);
			p_font->renderText(title, Vec2i(0, 0), Vec4d(0.75, 0.75, 1.0, alpha));

			glPopMatrix();
			glPushMatrix();

			glTranslated(320.0 + offset.x, 255.0 + offset.y, 0.0);
			glTranslated(texts[i].position.x, texts[i].position.y, 0.0);
			glTranslated(textSize.x / -2, 0.0, 0.0);

			p_font->renderText(text, Vec2i(0, 0), textColors[i % (sizeof(textColors) / sizeof(textColors[0]))] * Vec4d(1.0, 1.0, 1.0, alpha));

			glPopMatrix();
		}
	}

	glColorMask(true, true, true, true);

	double darkness = 0.0;
	if(t < 0.0) darkness = -0.5 * t;
	else if(t > 53.0) darkness = 0.5 * (t - 53.0);
	if(darkness > 0.0)
	{
		glDisable(GL_TEXTURE_2D);
		glBegin(GL_QUADS);
		glColor4d(0.0, 0.0, 0.0, darkness);
		glVertex2i(0, 0);
		glVertex2i(640, 0);
		glVertex2i(640, 480);
		glVertex2i(0, 480);
		glEnd();
		glEnable(GL_TEXTURE_2D);
	}
}

void GS_Credits::onUpdate()
{
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

	Engine& engine = Engine::inst();
	if(time == 55 * 1000) engine.playSound("character1.ogg");
	if(time == 56 * 1000) engine.playSound("character2.ogg");
	if(time == 57 * 1000) engine.playSound("character3.ogg");
	if(time == 58 * 1000) engine.setGameState("GS_Menu");

	if(engine.wasKeyPressed(SDLK_RETURN) ||
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
	p_sprites = p_level->getSprites();

	// Textur f¸r den Effekt-Puffer erzeugen
	glGenTextures(1, &bufferID);
	glBindTexture(GL_TEXTURE_2D, bufferID);
	const Vec2i& screenPow2Size = Engine::inst().getScreenPow2Size();
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, screenPow2Size.x, screenPow2Size.y, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

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
	glDeleteTextures(1, &bufferID);
	bufferID = 0;
}

void GS_Credits::onGetFocus()
{
	SDL_ShowCursor(0);
	Engine::inst().playMusic("credits.ogg", -1.0);
}

void GS_Credits::onLoseFocus()
{
	SDL_ShowCursor(1);
}

void GS_Credits::renderStars()
{
	p_sprites->bind();

	for(std::list<Star>::reverse_iterator i = stars.rbegin(); i != stars.rend(); ++i)
	{
		glPushMatrix();
		glTranslated(i->position.x, i->position.y, i->position.z);
		glScaled(i->size, i->size, i->size);
		glRotated(i->rotation.x, 1.0, 0.0, 0.0);
		glRotated(i->rotation.y, 0.0, 1.0, 0.0);
		glRotated(i->rotation.z, 0.0, 0.0, 1.0);

		double distSq = (i->position - cameraPos).lengthSq();
		double alpha = 1.0 / (1.0 + 0.001 * distSq);

		glBegin(GL_QUADS);
		glColor4d(1.0, 1.0, 1.0, alpha);
		glTexCoord2iv(i->positionOnTexture);
		glVertex2d(-0.5, 0.5);
		glTexCoord2iv(i->positionOnTexture + Vec2i(16, 0));
		glVertex2d(0.5, 0.5);
		glTexCoord2iv(i->positionOnTexture + Vec2i(16, 16));
		glVertex2d(0.5, -0.5);
		glTexCoord2iv(i->positionOnTexture + Vec2i(0, 16));
		glVertex2d(-0.5, -0.5);
		glEnd();

		glPopMatrix();
	}

	p_sprites->unbind();
}

void GS_Credits::updateStars()
{
	// Sterne entfernen, die nicht mehr sichtbar sind
	for(std::list<Star>::iterator i = stars.begin(); i != stars.end();)
	{
		i->rotation += 0.02 * i->deltaRotation;

		Vec3d d = i->position - cameraPos;
		double dot = d ^ cameraDir;
		if(dot <= 0.0) i = stars.erase(i);
		else i++;
	}

	// neue Sterne hinzuf¸gen
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