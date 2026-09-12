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

#ifdef __EMSCRIPTEN__
// Between the pulsing line and the fullscreen hint under it. Far enough that
// the two read as a message and an aside rather than as one paragraph.
static const int HINT_GAP = 16;
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
		// Gentle pulsing, or the line would read as a frozen still. The logo
		// stays off: its entrance belongs to the intro and only sets off
		// together with the jingle.
		const Vec4d color(1.0, 1.0, 1.0, 0.65 + 0.35 * sin(waitTime * 0.004));
		const std::string text = localizeString("$WEB_CLICK_TO_START");

		Vec2i dim;
		p_font->measureText(text, &dim, 0);

		// Centre each line on its own - after a break renderText starts again
		// at position.x, flush left.
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

		// A desktop browser offers no way to reach the game's own fullscreen,
		// and Alt+Enter is not a guess anybody makes, so it is said here - in
		// the tooltip font, because it is an aside and not the message. Not on
		// a phone: there the game takes the fullscreen itself on the first
		// touch, and there is no Alt to press anyway.
		Font* p_hintFont = GUI::inst().getToolTipFont();
		if(p_hintFont && !engine.isPhone())
		{
			const std::string hint = localizeString("$WEB_FULLSCREEN_HINT");
			Vec2i hintDim;
			p_hintFont->measureText(hint, &hintDim, 0);
			p_hintFont->renderText(hint, Vec2i(320 - hintDim.x / 2, y + HINT_GAP), color);
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

		GL::setTexturing(false);
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

		// Every input counts as a gesture. Emscripten hangs its own resume on
		// the first mousedown/keydown/touchstart but uses it up even when the
		// resume fails (once: true in autoResumeAudioContext), hence the
		// follow-up here. Only real mouse buttons (1-3); 4 and 5 are the
		// wheel, and the browser does not count scrolling as a gesture - it
		// would only arm the emergency brake below without unblocking the
		// audio.
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

		// Once the gesture is there it goes on in the same tick - it does not
		// wait for the audio. That is the difference between "the tap
		// registered" and "the screen is still blinking": resume() returns a
		// promise, and on a phone two seconds can pass before it settles,
		// while the line keeps pulsing and the player taps a second time. The
		// jingle loses nothing by it: it hangs off time >= 1000 and, for its
		// part, waits briefly for the audio below.
		//
		// Without a gesture it goes on as well once the audio is free of its
		// own accord: the click may have landed beside the canvas, where only
		// the browser saw it.
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
			// The context needs a few milliseconds after the gesture. A
			// second of slack has passed here, which is almost always
			// enough; if not, it waits until 2000 and then gives up, or the
			// jingle would fire only as the menu comes up.
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

	// -nosplash does not request logo and jingle in the first place.
	// Everything else follows by itself: without a logo, time starts at 3000
	// and the whole intro falls away - the same path the game takes anyway
	// when logo.png will not load.
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

	// Without a logo the jingle would otherwise still play: time is already
	// over the threshold and the first tick fires it. -nosplash does not want
	// that; where only the file is missing, the jingle still plays.
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
	// load the images
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
	// load the sounds
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