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
// Between the pulsing line and the fullscreen hint under it: enough that the
// two read as a message and an aside, not as one paragraph.
static const int HINT_GAP = 16;
#endif

GS_Loading::GS_Loading() : GameState("GS_Loading"), engine(Engine::inst())
{
	p_font = 0;
	p_logo = 0;
}

GS_Loading::~GS_Loading()
{
}

void GS_Loading::onRender()
{
	Renderer& renderer = Renderer::inst();
	renderer.clear(Vec4f(0.0f, 0.0f, 0.0f, 0.0f));

#ifdef __EMSCRIPTEN__
	if(waitingForClick)
	{
		// Gentle pulsing, or the line would read as a frozen still. No logo
		// yet: its entrance belongs to the intro and starts with the jingle.
		const Vec4f color(1.0f, 1.0f, 1.0f, 0.65f + 0.35f * sinf(waitTime * 0.004f));
		const std::string text = localizeString("$WEB_CLICK_TO_START");

		Vec2i dim;
		p_font->measureText(text, &dim, 0);

		// Each line centred on its own: after a break renderText starts again
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

		// The first gesture takes the fullscreen, but nobody guesses that
		// Alt+Enter is the way back into it, so it is said here, in the tooltip
		// font as an aside. Not where the on-screen pad is up: that has a
		// button for it, and there is no Alt to press.
		Font* p_hintFont = GUI::inst().getToolTipFont();
		if(p_hintFont && !engine.isPadShown())
		{
			const std::string hint = localizeString("$WEB_FULLSCREEN_HINT");
			Vec2i hintDim;
			p_hintFont->measureText(hint, &hintDim, 0);
			p_hintFont->renderText(hint, Vec2i(320 - hintDim.x / 2, y + HINT_GAP), color);
		}

		return;
	}
#endif

	renderer.push();
	renderer.loadIdentity();
	renderer.translate(320.0f, 220.0f);
	renderer.scale(logoSize, logoSize);

	if(p_logo)
	{
		const Vec2f corners[4] = {Vec2f(-256.0f, -256.0f), Vec2f(256.0f, -256.0f), Vec2f(256.0f, 256.0f), Vec2f(-256.0f, 256.0f)};
		const Vec2f uvs[4] = {Vec2f(0.0f, 0.0f), Vec2f(512.0f, 0.0f), Vec2f(512.0f, 512.0f), Vec2f(0.0f, 512.0f)};
		renderer.setTexture(p_logo->ref());
		renderer.quad(renderer.state(), corners, uvs, Vec4f(1.0f, 1.0f, 1.0f, 1.0f));
	}

	renderer.pop();

	if(time >= 2900)
	{
		std::string text = localizeString("$LOADING");
		Vec2i dim;
		p_font->measureText(text, &dim, 0);
		p_font->renderText(text, Vec2i(320 - dim.x / 2, 440), Vec4f(1.0f));
	}
}

void GS_Loading::onUpdate()
{
#ifdef __EMSCRIPTEN__
	if(waitingForClick)
	{
		waitTime += 20;

		// Every input counts as a gesture. Emscripten's own resume hangs off
		// the first mousedown/keydown/touchstart and is spent even when it
		// fails (once: true in autoResumeAudioContext), hence the follow-up
		// here. Only mouse buttons 1-3: 4 and 5 are the wheel, which the
		// browser does not count as a gesture, so it would end the wait below
		// without unblocking the audio.
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

		// A gesture goes on in the same tick without waiting for the audio:
		// resume() returns a promise that can take two seconds to settle on a
		// phone, with the line still pulsing and the player tapping again. The
		// jingle, at time >= 1000, waits briefly for the audio itself.
		//
		// Without a gesture it goes on once the audio is free of its own
		// accord: the click may have landed beside the canvas, where only the
		// browser saw it.
		if(gestureTime >= 0 || !WebAudio::isSuspended()) waitingForClick = false;

		return;
	}
#endif

	time += 20;

	// The clock the frame oracle runs on, as Level::update and the credits
	// report theirs; the logo, the jingle and the loading line hang off it.
	engine.sceneTick = static_cast<uint>(time);

	if(time >= 1000)
	{
		if(!soundPlayed)
		{
#ifdef __EMSCRIPTEN__
			// The context needs a few milliseconds after the gesture, and
			// the second that has passed is almost always enough. Otherwise
			// it waits until 2000 and gives up, or the jingle would fire
			// only as the menu comes up.
			const bool ready = !WebAudio::isSuspended();
			if(ready) engine.playSound("logo.ogg");
			if(ready || time >= 2000) soundPlayed = true;
#else
			engine.playSound("logo.ogg");
			soundPlayed = true;
#endif
		}

		logoSizeVel += 0.02f * 80.0f * (1.0f - logoSize);
		logoSize += 0.02f * logoSizeVel;
		logoSizeVel *= 0.8f;
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
			engine.crossfade(new CF_Mosaic, 1.0f);
			load = 3;
		}
	}
}

void GS_Loading::onEnter(const ParameterBlock& context)
{
	p_font = GUI::inst().getFont();

	// -nosplash does not request logo and jingle at all. The rest follows:
	// without a logo time starts at 3000 and the intro falls away, the same
	// path the game takes when logo.png will not load.
	const bool skipSplash = Engine::inst().isSplashSkipped();
	p_logo = 0;
	if(!skipSplash)
	{
		p_logo = Manager<Texture>::inst().request("logo.png", Texture::WM_CLAMP | Texture::NEVER_PACK);
		Manager<Sound>::inst().request("logo.ogg");
	}

	if(p_logo) time = 0;
	else time = 3000;
	logoSize = 0.0f;
	logoSizeVel = 0.0f;
	load = 0;

	// Without a logo time is already past the jingle's threshold, and the
	// first tick would play it. -nosplash does not want that; where only the
	// file is missing, the jingle still plays.
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
	sndMgr.request("hint.ogg");
	sndMgr.request("hintscroll.ogg");
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