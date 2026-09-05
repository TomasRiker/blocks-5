#include "pch.h"
#include "gs_loading.h"
#include "engine.h"
#include "font.h"
#include "texture.h"
#include "sound.h"
#include "gui.h"
#include "cf_all.h"
#ifdef __EMSCRIPTEN__
#include "web_audio.h"
#endif

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

#ifdef __EMSCRIPTEN__
	if(waitingForClick)
	{
		// Sanftes Pulsieren, damit die Zeile nicht wie ein eingefrorenes
		// Standbild wirkt. Das Logo bleibt aus: sein Auftritt gehoert zum
		// Vorspann und laeuft erst mit dem Jingle zusammen los.
		const Vec4d color(1.0, 1.0, 1.0, 0.65 + 0.35 * sin(waitTime * 0.004));
		const std::string text = localizeString("$WEB_CLICK_TO_START");

		Vec2i dim;
		p_font->measureText(text, &dim, 0);

		// Jede Zeile fuer sich zentrieren - renderText setzt nach einem
		// Umbruch wieder bei position.x an, also linksbuendig.
		int y = 240 - dim.y / 2;
		for(size_t begin = 0; begin <= text.length(); )
		{
			size_t end = text.find_first_of("\n\xB6", begin);
			if(end == std::string::npos) end = text.length();

			const std::string line = text.substr(begin, end - begin);
			Vec2i lineDim;
			p_font->measureText(line, &lineDim, 0);
			p_font->renderText(line, Vec2i(320 - lineDim.x / 2, y), color);

			y += lineDim.y;
			begin = end + 1;
		}

		return;
	}
#endif

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
#ifdef __EMSCRIPTEN__
	if(waitingForClick)
	{
		waitTime += 20;

		// Jede Eingabe zaehlt als Geste. Emscripten haengt selbst einen
		// Aufwecker an das erste mousedown/keydown/touchstart, verbraucht ihn
		// aber auch dann, wenn das Aufwecken scheitert (once: true in
		// autoResumeAudioContext) - hier wird deshalb nachgefasst.
		// Nur echte Tasten (1-3); 4 und 5 sind das Mausrad, und Scrollen
		// gilt dem Browser nicht als Geste - es wuerde also nur die
		// Notbremse unten scharf machen, ohne den Ton freizuschalten.
		bool input = false;
		for(uint button = SDL_BUTTON_LEFT; button <= SDL_BUTTON_RIGHT; button++)
			if(engine.wasButtonPressed(button)) input = true;

		SDL_KeyboardEvent keyEvent;
		while(engine.getKeyEvent(&keyEvent))
			if(keyEvent.type == SDL_KEYDOWN) input = true;

		if(input)
		{
			WebAudio::resume();
			if(gestureTime < 0) gestureTime = waitTime;
		}

		// Sobald die Geste da ist, geht es im selben Takt weiter - auf den Ton
		// wird nicht gewartet. Das ist der Unterschied zwischen "der Tipp ist
		// angekommen" und "der Bildschirm blinkt noch": resume() liefert ein
		// Versprechen, und bis es eingeloest ist, koennen auf einem Telefon
		// zwei Sekunden vergehen, in denen die Zeile weiterpulsiert und der
		// Spieler ein zweites Mal tippt. Der Jingle verliert dabei nichts, er
		// haengt an time >= 1000 und wartet unten seinerseits kurz auf den Ton.
		//
		// Ohne Geste geht es auch weiter, sobald der Ton von sich aus frei ist:
		// der Klick kann neben die Zeichenflaeche gegangen sein, dann hat ihn
		// nur der Browser gesehen.
		if(gestureTime >= 0 || !WebAudio::isSuspended()) waitingForClick = false;

		return;
	}
#endif

	time += 20;

	if(time >= 1000)
	{
		if(!soundPlayed)
		{
#ifdef __EMSCRIPTEN__
			// Der Kontext braucht nach der Geste ein paar Millisekunden. Hier
			// ist eine Sekunde Vorlauf vergangen, das reicht fast immer; wenn
			// nicht, wird bis 2000 gewartet und danach aufgegeben, damit der
			// Jingle nicht erst zum Menue hin losgeht.
			const bool ready = !WebAudio::isSuspended();
			if(ready) engine.playSound("logo.ogg");
			if(ready || time >= 2000) soundPlayed = true;
#else
			engine.playSound("logo.ogg");
			soundPlayed = true;
#endif
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

	// -nosplash holt Logo und Jingle gar nicht erst. Alles Weitere ergibt
	// sich von selbst: ohne Logo faengt time schon bei 3000 an, und damit
	// faellt der ganze Vorspann weg - derselbe Weg, den das Spiel ohnehin
	// nimmt, wenn sich logo.png nicht laden laesst.
	const bool skipSplash = Engine::inst().isSplashSkipped();
	p_logo = 0;
	if(!skipSplash)
	{
		p_logo = Manager<Texture>::inst().request("logo.png");
		Manager<Sound>::inst().request("logo.ogg");
	}

	if(p_logo) time = 0;
	else time = 3000;
	logoSize = 0.0;
	logoSizeVel = 0.0;
	load = 0;

	// Ohne Logo spielt der Jingle sonst trotzdem: time steht dann schon ueber
	// der Schwelle, und der erste Takt loest ihn aus. Bei -nosplash ist das
	// nicht gewollt; fehlt nur die Datei, bleibt es beim bisherigen Verhalten.
	soundPlayed = skipSplash;

#ifdef __EMSCRIPTEN__
	waitingForClick = WebAudio::isSuspended();
	waitTime = 0;
	gestureTime = -1;
#endif
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
	sndMgr.request("rewind.ogg");
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