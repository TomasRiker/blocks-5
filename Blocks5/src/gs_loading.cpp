#include "pch.h"
#include "gs_loading.h"
#include "engine.h"
#include "font.h"
#include "texture.h"
#include "sound.h"
#include "gui.h"
#include "cf_all.h"

GS_Loading::GS_Loading() : GameState("GS_Loading"), engine(Engine::inst())
{
}

GS_Loading::~GS_Loading()
{
}

void GS_Loading::onRender()
{
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	glPushMatrix();
	glLoadIdentity();
	glTranslated(320.0, 220.0, 0.0);
	glScaled(logoSize, logoSize, 1.0);

	if(p_logo)
	{
		p_logo->bind();

		glBegin(GL_QUADS);
		glColor4d(1.0, 1.0, 1.0, 1.0);
		glTexCoord2i(0, 0);
		glVertex2i(-256, -256);
		glTexCoord2i(512, 0);
		glVertex2i(256, -256);
		glTexCoord2i(512, 512);
		glVertex2i(256, 256);
		glTexCoord2i(0, 512);
		glVertex2i(-256, 256);
		glEnd();

		p_logo->unbind();
	}

	glPopMatrix();

	if(time >= 2900)
	{
		std::string text = localizeString("$LOADING");
		Vec2i dim;
		p_font->measureText(text, &dim, 0);
		p_font->renderText(text, Vec2i(320 - dim.x / 2, 440), Vec4d(1.0));
	}
}

void GS_Loading::onUpdate()
{
	time += 20;

	if(time >= 1000)
	{
		if(!soundPlayed)
		{
			engine.playSound("logo.ogg");
			soundPlayed = true;
		}

		logoSizeVel += 0.02 * 80.0 * (1.0 - logoSize);
		logoSize += 0.02 * logoSizeVel;
		logoSizeVel *= 0.8;
	}

	if(time >= 3000)
	{
		if(load == 0)
		{
			loadGraphics();
			load = 1;
		}
		else if(load == 1)
		{
			loadSounds();
			load = 2;
		}
		else if(load == 2)
		{
			engine.setGameState("GS_Menu");
			engine.crossfade(new CF_Mosaic, 1.0);
			load = 3;
		}
	}
}

void GS_Loading::onEnter(const ParameterBlock& context)
{
	p_font = GUI::inst().getFont();
	p_logo = Manager<Texture>::inst().request("logo.png");
	Manager<Sound>::inst().request("logo.ogg");

	if(p_logo) time = 0;
	else time = 3000;
	logoSize = 0.0;
	logoSizeVel = 0.0;
	load = 0;
	soundPlayed = false;
}

void GS_Loading::onLeave(const ParameterBlock& context)
{
	if(p_logo)
	{
		p_logo->release();
		p_logo = 0;
	}
}

void GS_Loading::onGetFocus()
{
}

void GS_Loading::onLoseFocus()
{
}

void GS_Loading::loadGraphics()
{
	// Bilder laden
	printfLog("Loading graphics ...\n");
	Manager<Texture>& texMgr = Manager<Texture>::inst();
	texMgr.request("title.png");
	Texture* p_misc = texMgr.request("misc.png");
	texMgr.request("icons.png");
	texMgr.request("languages.png");
	texMgr.request("lightning.png");
	texMgr.request("lava_edges.png");

	engine.setMuteIcon(p_misc, Vec2i(50, 0), Vec2i(38, 38));
	engine.setRecordingIcon(p_misc, Vec2i(90, 0), Vec2i(48, 16));
}

void GS_Loading::loadSounds()
{
	// Sounds laden
	printfLog("Loading sounds ...\n");
	Manager<Sound>& sndMgr = Manager<Sound>::inst();
	sndMgr.request("barrageswitch.ogg");
	sndMgr.request("barrageswitch_failed.ogg");
	sndMgr.request("block.ogg");
	sndMgr.request("bomb.ogg");
	sndMgr.request("bomb_plant.ogg");
	sndMgr.request("cannon_fire.ogg");
	sndMgr.request("cannon_turn.ogg");
	sndMgr.request("character1.ogg");
	sndMgr.request("character2.ogg");
	sndMgr.request("character3.ogg");
	sndMgr.request("conveyorbelt.ogg");
	sndMgr.request("destroy.ogg");
	sndMgr.request("diamond.ogg");
	sndMgr.request("diamondmachine.ogg");
	sndMgr.request("e_valueswitch_off.ogg");
	sndMgr.request("e_valueswitch_on.ogg");
	sndMgr.request("electricityswitch.ogg");
	sndMgr.request("elevator.ogg");
	sndMgr.request("enemy1_burp1.ogg");
	sndMgr.request("enemy1_burp2.ogg");
	sndMgr.request("enemy1_eat.ogg");
	sndMgr.request("enemy1_turn.ogg");
	sndMgr.request("enemy2_eat.ogg");
	sndMgr.request("enemy2_growl.ogg");
	sndMgr.request("enemy2_laugh.ogg");
	sndMgr.request("enemy_burst.ogg");
	sndMgr.request("exit.ogg");
	sndMgr.request("explosion.ogg");
	sndMgr.request("falling.ogg");
	sndMgr.request("finished.ogg");
	sndMgr.request("gas.ogg");
	sndMgr.request("geiger.ogg");
	sndMgr.request("grass.ogg");
	sndMgr.request("hotel.ogg");
	sndMgr.request("laser.ogg");
	sndMgr.request("light_off.ogg");
	sndMgr.request("light_on.ogg");
	sndMgr.request("magnet.ogg");
	sndMgr.request("mask.ogg");
	sndMgr.request("player_burst.ogg");
	sndMgr.request("push.ogg");
	sndMgr.request("rain.ogg");
	sndMgr.request("ricochet.ogg");
	sndMgr.request("screenshot.ogg");
	sndMgr.request("syringe.ogg");
	sndMgr.request("teleport_begin.ogg");
	sndMgr.request("teleport_end.ogg");
	sndMgr.request("teleport_failed.ogg");
	sndMgr.request("thunder.ogg");
	sndMgr.request("thunderstorm.ogg");
	sndMgr.request("toxic.ogg");
	sndMgr.request("vaporize.ogg");
}