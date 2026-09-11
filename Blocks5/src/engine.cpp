#include "pch.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#include "web_audio.h"
// Defined further down, next to setFullScreen().
static EM_BOOL engineFullScreenHotkey(int, const EmscriptenKeyboardEvent*, void*);
static EM_BOOL engineTouchFullScreen(int, const EmscriptenTouchEvent*, void*);
#endif
#ifdef _WIN32
// For the fullscreen switch: the window style is set directly, bypassing SDL.
#include <SDL_syswm.h>
#elif !defined(__EMSCRIPTEN__)
// The same for X11, but in a translation unit of its own: <X11/Xlib.h> takes
// Font, Window, Screen and Cursor as type names of its own, and the game's
// classes are called exactly that.
#include "linux_window.h"
#endif
#include "engine.h"
#include "glextensions.h"
#include "testhooks.h"
#include "u_all.h"
#ifdef __EMSCRIPTEN__
#include "web_bluescreen.h"
#include "web_transfer.h"
#endif
#include "gamestate.h"
#include "soundinstance.h"
#include "texture.h"
#include "sprite.h"
#include "font.h"
#include "sound.h"
#include "streamedsound.h"
#include "gui.h"
#include "tileset.h"
#include "crossfade.h"
#include "filesystem.h"
#include "img_save.h"
#include "videorecorder.h"
#include "audiocapture.h"

// Headroom for the audio mix. The game plays music and a dozen effects, each
// source at full volume, and the sum stood above the ceiling: measured in the
// menu, where the demo keeps adding bombs and lasers, -8.8 LUFS at a peak of
// 0 dBFS - and 0.73% of all samples lay against the limit and were therefore
// clipped by OpenAL Soft. That is audible as distortion, in the game and in
// a recording alike.
//
// 0.45 brings that to -15.5 LUFS at a peak of -0.9 dBFS. Two standards decide
// the number: a peak no higher than -1 dBTP, because a lossy encoder - MP3
// for the videos here - can overshoot on decoding, and a loudness of -14 to
// -16 LUFS, which is where the video portals normalise anyway. Measured
// afterwards: exactly one single sample out of four million against the limit
// instead of 29369.
//
// The value stands here and not in the options: it is a property of the
// mixture, not a matter of taste. The player's own sliders are untouched and
// still read 100%.
const double MASTER_HEADROOM = 0.45;

Engine::Engine()
{
	initialized = false;
	appActive = true;

	for(int i = 0; i < NUM_KEY_SLOTS; i++)
	{
		keyData[i] = 0;
		keyHeld[i] = false;
		buttonData[i] = 0;
	}

	frameTime = 0;
	time = 0;
	grabbingKey = false;
	grabResult = GRAB_WAITING;
	grabDeadline = 0;
	grabHasDeadline = false;
	p_crossfade = 0;
	crossfadeTime = -1.0;
	crossfadeDuration = 0.0;
	glExtBlendFuncSeparate = 0;
	p_videoRecorder = 0;
	p_audioCapture = 0;
	p_muteIconTexture = 0;
	p_cursor1x = 0;
	p_cursor2x = 0;
	cursorScale = 0;
	frameBufferID = 0;
	frameTextureID = 0;
	frameDepthStencilID = 0;
	renderTargetID = 0;
	renderTargetScissor = false;
	presentVertexBuffer = 0;
	useFrameBuffer = false;
	// The four filters. They stand before loadConfig(), which looks one of them
	// up by name, and that is long before the GL context; their GL state comes
	// into being only in createUpscalerGL(). The order is the options dialog's:
	// the best first, the matter of style last.
	p_sharpFit = new U_SharpFit();
	p_sharp    = new U_Sharp();
	p_smooth   = new U_Smooth();
	p_crt      = new U_Crt();
	upscalers.push_back(p_sharpFit);
	upscalers.push_back(p_sharp);
	upscalers.push_back(p_smooth);
	upscalers.push_back(p_crt);
	p_wantedUpscaler = p_sharpFit;   // without shaders this becomes Sharp
	fullScreen = false;
	fullScreenOverride = -1;
	splashSkipped = false;
	frameBufferDisabled = false;
	shadersDisabled = false;
	performanceShown = false;
	renderSuppressed = false;
	renderSuppressWanted = false;
	spriteBatchOpen = false;
	spriteBatchDisabled = false;
	batchTexture = 0;
	batchTexturing = GL_FALSE;
	lastFrameBegin = 0.0;
	swallowedReturn = false;
	windowedSize = Vec2i(0, 0);      // 0 = nothing chosen yet, init() decides
	windowedPosition = Vec2i(0, 0);
	windowedPositionKnown = false;
	maximized = false;
#ifdef _WIN32
	inSizeMove = false;
#endif
	savedWindowStyle = 0;
	savedWindowRect[0] = savedWindowRect[1] = savedWindowRect[2] = savedWindowRect[3] = 0;
	oldSoundVolume = -1.0;
	oldMusicVolume = -1.0;
	timePlayed = 0;
	doScreenshot = false;
}

Engine::~Engine()
{
	exit();

	// The GL state fell in exit(); here only the objects fall.
	for(std::vector<Upscaler*>::iterator i = upscalers.begin(); i != upscalers.end(); ++i)
	{
		delete *i;
	}
	upscalers.clear();
	p_sharp = 0;
	p_smooth = 0;
	p_sharpFit = 0;
	p_crt = 0;
	p_wantedUpscaler = 0;
}

namespace
{
	// What a key is called in config.xml. The number is no good for that:
	// SDLK_LEFT is 276 under SDL 1.2 and 1104 in the browser.
	struct KeyName
	{
		const char* p_name;
		int         key;
	};

	const KeyName p_keyNames[] =
	{
		{"UNKNOWN", SDLK_UNKNOWN}     , {"BACKSPACE", SDLK_BACKSPACE} , {"TAB", SDLK_TAB},
		{"CLEAR", SDLK_CLEAR}         , {"RETURN", SDLK_RETURN}       , {"PAUSE", SDLK_PAUSE},
		{"ESCAPE", SDLK_ESCAPE}       , {"SPACE", SDLK_SPACE}         , {"EXCLAIM", SDLK_EXCLAIM},
		{"QUOTEDBL", SDLK_QUOTEDBL}   , {"HASH", SDLK_HASH}           , {"DOLLAR", SDLK_DOLLAR},
		{"AMPERSAND", SDLK_AMPERSAND} , {"QUOTE", SDLK_QUOTE}         , {"LEFTPAREN", SDLK_LEFTPAREN},
		{"RIGHTPAREN", SDLK_RIGHTPAREN}, {"ASTERISK", SDLK_ASTERISK}   , {"PLUS", SDLK_PLUS},
		{"COMMA", SDLK_COMMA}         , {"MINUS", SDLK_MINUS}         , {"PERIOD", SDLK_PERIOD},
		{"SLASH", SDLK_SLASH}         , {"0", SDLK_0}                 , {"1", SDLK_1},
		{"2", SDLK_2}                 , {"3", SDLK_3}                 , {"4", SDLK_4},
		{"5", SDLK_5}                 , {"6", SDLK_6}                 , {"7", SDLK_7},
		{"8", SDLK_8}                 , {"9", SDLK_9}                 , {"COLON", SDLK_COLON},
		{"SEMICOLON", SDLK_SEMICOLON} , {"LESS", SDLK_LESS}           , {"EQUALS", SDLK_EQUALS},
		{"GREATER", SDLK_GREATER}     , {"QUESTION", SDLK_QUESTION}   , {"AT", SDLK_AT},
		{"LEFTBRACKET", SDLK_LEFTBRACKET}, {"BACKSLASH", SDLK_BACKSLASH} , {"RIGHTBRACKET", SDLK_RIGHTBRACKET},
		{"CARET", SDLK_CARET}         , {"UNDERSCORE", SDLK_UNDERSCORE}, {"BACKQUOTE", SDLK_BACKQUOTE},
		{"a", SDLK_a}                 , {"b", SDLK_b}                 , {"c", SDLK_c},
		{"d", SDLK_d}                 , {"e", SDLK_e}                 , {"f", SDLK_f},
		{"g", SDLK_g}                 , {"h", SDLK_h}                 , {"i", SDLK_i},
		{"j", SDLK_j}                 , {"k", SDLK_k}                 , {"l", SDLK_l},
		{"m", SDLK_m}                 , {"n", SDLK_n}                 , {"o", SDLK_o},
		{"p", SDLK_p}                 , {"q", SDLK_q}                 , {"r", SDLK_r},
		{"s", SDLK_s}                 , {"t", SDLK_t}                 , {"u", SDLK_u},
		{"v", SDLK_v}                 , {"w", SDLK_w}                 , {"x", SDLK_x},
		{"y", SDLK_y}                 , {"z", SDLK_z}                 , {"DELETE", SDLK_DELETE},
		{"KP0", SDLK_KP0}             , {"KP1", SDLK_KP1}             , {"KP2", SDLK_KP2},
		{"KP3", SDLK_KP3}             , {"KP4", SDLK_KP4}             , {"KP5", SDLK_KP5},
		{"KP6", SDLK_KP6}             , {"KP7", SDLK_KP7}             , {"KP8", SDLK_KP8},
		{"KP9", SDLK_KP9}             , {"KP_PERIOD", SDLK_KP_PERIOD} , {"KP_DIVIDE", SDLK_KP_DIVIDE},
		{"KP_MULTIPLY", SDLK_KP_MULTIPLY}, {"KP_MINUS", SDLK_KP_MINUS}   , {"KP_PLUS", SDLK_KP_PLUS},
		{"KP_ENTER", SDLK_KP_ENTER}   , {"KP_EQUALS", SDLK_KP_EQUALS} , {"UP", SDLK_UP},
		{"DOWN", SDLK_DOWN}           , {"RIGHT", SDLK_RIGHT}         , {"LEFT", SDLK_LEFT},
		{"INSERT", SDLK_INSERT}       , {"HOME", SDLK_HOME}           , {"END", SDLK_END},
		{"PAGEUP", SDLK_PAGEUP}       , {"PAGEDOWN", SDLK_PAGEDOWN}   , {"F1", SDLK_F1},
		{"F2", SDLK_F2}               , {"F3", SDLK_F3}               , {"F4", SDLK_F4},
		{"F5", SDLK_F5}               , {"F6", SDLK_F6}               , {"F7", SDLK_F7},
		{"F8", SDLK_F8}               , {"F9", SDLK_F9}               , {"F10", SDLK_F10},
		{"F11", SDLK_F11}             , {"F12", SDLK_F12}             , {"F13", SDLK_F13},
		{"F14", SDLK_F14}             , {"F15", SDLK_F15}             , {"NUMLOCK", SDLK_NUMLOCK},
		{"CAPSLOCK", SDLK_CAPSLOCK}   , {"SCROLLOCK", SDLK_SCROLLOCK} , {"RSHIFT", SDLK_RSHIFT},
		{"LSHIFT", SDLK_LSHIFT}       , {"RCTRL", SDLK_RCTRL}         , {"LCTRL", SDLK_LCTRL},
		{"RALT", SDLK_RALT}           , {"LALT", SDLK_LALT}           , {"RMETA", SDLK_RMETA},
		{"LMETA", SDLK_LMETA}         , {"LSUPER", SDLK_LSUPER}       , {"RSUPER", SDLK_RSUPER},
		{"MODE", SDLK_MODE}           , {"COMPOSE", SDLK_COMPOSE}     , {"HELP", SDLK_HELP},
		{"PRINT", SDLK_PRINT}         , {"SYSREQ", SDLK_SYSREQ}       , {"BREAK", SDLK_BREAK},
		{"MENU", SDLK_MENU}           , {"POWER", SDLK_POWER}         , {"EURO", SDLK_EURO},
		{"UNDO", SDLK_UNDO},
	};

	std::string keyboardVKId(int key)
	{
		for(uint i = 0; i < sizeof(p_keyNames) / sizeof(p_keyNames[0]); i++)
		{
			if(p_keyNames[i].key == key) return std::string("key:") + p_keyNames[i].p_name;
		}

		// If this build knows the key but the table does not: as a number.
		char temp[32] = "";
		sprintf(temp, "key:#%d", key);
		return temp;
	}

	// What a key is called where the player reads it, as opposed to the id
	// above, which config.xml holds and which can never be translated. Only the
	// keys somebody is likely to bind are in here; everything else falls back
	// to SDL's own name.
	//
	// The ids are spelled out and not composed from the key name, because
	// verify.py collects "$..." literals out of the source: an id that
	// languages.txt does not have is then a failed check rather than a player
	// reading "$VK_KEYBOARD_LEFT" off a button.
	const struct { int key; const char* p_id; } p_keyDisplayNames[] =
	{
		{SDLK_LEFT, "$VK_KEYBOARD_LEFT"},           {SDLK_RIGHT, "$VK_KEYBOARD_RIGHT"},
		{SDLK_UP, "$VK_KEYBOARD_UP"},               {SDLK_DOWN, "$VK_KEYBOARD_DOWN"},
		{SDLK_RETURN, "$VK_KEYBOARD_RETURN"},       {SDLK_KP_ENTER, "$VK_KEYBOARD_KP_ENTER"},
		{SDLK_ESCAPE, "$VK_KEYBOARD_ESCAPE"},       {SDLK_SPACE, "$VK_KEYBOARD_SPACE"},
		{SDLK_TAB, "$VK_KEYBOARD_TAB"},             {SDLK_BACKSPACE, "$VK_KEYBOARD_BACKSPACE"},
		{SDLK_DELETE, "$VK_KEYBOARD_DELETE"},       {SDLK_INSERT, "$VK_KEYBOARD_INSERT"},
		{SDLK_HOME, "$VK_KEYBOARD_HOME"},           {SDLK_END, "$VK_KEYBOARD_END"},
		{SDLK_PAGEUP, "$VK_KEYBOARD_PAGEUP"},       {SDLK_PAGEDOWN, "$VK_KEYBOARD_PAGEDOWN"},
		{SDLK_LSHIFT, "$VK_KEYBOARD_LSHIFT"},       {SDLK_RSHIFT, "$VK_KEYBOARD_RSHIFT"},
		{SDLK_LCTRL, "$VK_KEYBOARD_LCTRL"},         {SDLK_RCTRL, "$VK_KEYBOARD_RCTRL"},
		{SDLK_LALT, "$VK_KEYBOARD_LALT"},           {SDLK_RALT, "$VK_KEYBOARD_RALT"},
		{SDLK_PAUSE, "$VK_KEYBOARD_PAUSE"},
		{SDLK_KP0, "$VK_KEYBOARD_KP0"},             {SDLK_KP1, "$VK_KEYBOARD_KP1"},
		{SDLK_KP2, "$VK_KEYBOARD_KP2"},             {SDLK_KP3, "$VK_KEYBOARD_KP3"},
		{SDLK_KP4, "$VK_KEYBOARD_KP4"},             {SDLK_KP5, "$VK_KEYBOARD_KP5"},
		{SDLK_KP6, "$VK_KEYBOARD_KP6"},             {SDLK_KP7, "$VK_KEYBOARD_KP7"},
		{SDLK_KP8, "$VK_KEYBOARD_KP8"},             {SDLK_KP9, "$VK_KEYBOARD_KP9"},
		// These five need an entry more than the rest do: SDL names them
		// "[+]", "[-]", "[*]", "[/]" and "[.]", and a keycap drawn around a
		// pair of brackets reads as a fault rather than as a key.
		{SDLK_KP_PLUS, "$VK_KEYBOARD_KP_PLUS"},     {SDLK_KP_MINUS, "$VK_KEYBOARD_KP_MINUS"},
		{SDLK_KP_MULTIPLY, "$VK_KEYBOARD_KP_MULTIPLY"},
		{SDLK_KP_DIVIDE, "$VK_KEYBOARD_KP_DIVIDE"}, {SDLK_KP_PERIOD, "$VK_KEYBOARD_KP_PERIOD"},
	};

	std::string keyboardNiceName(int key, const char* p_sdlName)
	{
		for(uint i = 0; i < sizeof(p_keyDisplayNames) / sizeof(p_keyDisplayNames[0]); i++)
		{
			if(p_keyDisplayNames[i].key == key) return p_keyDisplayNames[i].p_id;
		}

		// SDL's own name otherwise, with the first letter raised: it hands back
		// "f5" and "a", and a button captioned "f5" reads as a fault. The
		// function keys need nothing more than this.
		std::string name = p_sdlName ? p_sdlName : "";
		if(name.empty()) return "?";
		name[0] = static_cast<char>(toupper(static_cast<unsigned char>(name[0])));
		return name;
	}
}

bool Engine::init(const std::string& windowCaption,
				  const std::string& windowIconFilename,
				  uint width,
				  uint height,
				  bool defaultFullScreen)
{
	if(initialized) return false;

	screenSize = Vec2i(width, height);
	screenPow2Size = Vec2i(nextPow2(width), nextPow2(height));

	// Order: default, config.xml, command line. From here on only fullScreen
	// counts, never defaultFullScreen again.
	fullScreen = defaultFullScreen;
#ifdef __EMSCRIPTEN__
	// The Fullscreen API cannot be triggered without a real key press; in the
	// browser there is therefore no fullscreen at startup.
	fullScreen = false;
#endif
	// Load the configuration. Sets windowedSize if the file says anything
	// about it.
	loadConfig();

	if(fullScreenOverride >= 0) fullScreen = (fullScreenOverride != 0);

	printfLog("* Language: %s\n", language.c_str());

	// initialize SDL
	printfLog("* Initializing SDL ...\n");
	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_JOYSTICK))
	{
		printfLog("+ ERROR: %s\n", SDL_GetError());
		return false;
	}

	SDL_WM_SetCaption(windowCaption.c_str(), windowCaption.c_str());
	SDL_EnableKeyRepeat(140, 60);
	SDL_EnableUNICODE(1);

	// add every key as a VK
	for(int k = 0; k < SDLK_LAST; k++)
	{
		VirtualKey vk;
		const char* p_name = SDL_GetKeyName(static_cast<SDLKey>(k));
		vk.name = std::string("Keyboard ") + (p_name ? p_name : "???");
		vk.niceName = keyboardNiceName(k, p_name);
		vk.id = keyboardVKId(k);
		vk.key = k;
		vk.down = false;
		virtualKeys.push_back(vk);
	}

	// open every joystick
	int n = SDL_NumJoysticks();
	int index = 0;
	for(int j = 0; j < n; j++)
	{
		SDL_Joystick* p_joystick = SDL_JoystickOpen(j);
		if(p_joystick)
		{
			// add every button as a VK
			int nk = SDL_JoystickNumButtons(p_joystick);
			for(int k = 0; k < nk; k++)
			{
				VirtualKey vk;
				std::ostringstream str;
				str << index + 1 << " B" << k + 1;
				vk.niceName = str.str();
				vk.name = "Joystick" + vk.niceName;
				vk.id = vk.name;
				vk.device = index;
				vk.key = k;
				vk.down = false;
				virtualKeys.push_back(vk);
			}

			// add every axis as a VK
			int na = SDL_JoystickNumAxes(p_joystick);
			for(int a = 0; a < na; a++)
			{
				VirtualKey vk;

				std::ostringstream str;
				str << index + 1 << " A" << a + 1 << "-";
				vk.niceName = str.str();
				vk.name = "Joystick" + vk.niceName;
				vk.id = vk.name;
				vk.device = index;
				vk.axis = a;
				vk.positive = false;
				virtualKeys.push_back(vk);

				str.str("");
				str << index + 1 << " A" << a + 1 << "+";
				vk.niceName = str.str();
				vk.name = "Joystick" + vk.niceName;
				vk.id = vk.name;
				vk.device = index;
				vk.axis = a;
				vk.positive = true;
				virtualKeys.push_back(vk);
			}

			// add all hats with all directions as VKs
			int nh = SDL_JoystickNumHats(p_joystick);
			for(int h = 0; h < nh; ++h)
			{
				VirtualKey vk;

				std::ostringstream str;
				str << index + 1 << " H" << h + 1;
				vk.device = index;
				vk.hat = h;

				vk.niceName = str.str() + "N";
				vk.name = "Joystick" + vk.niceName;
				vk.id = vk.name;
				vk.hatDir = SDL_HAT_UP;
				virtualKeys.push_back(vk);

				vk.niceName = str.str() + "NE";
				vk.name = "Joystick" + vk.niceName;
				vk.id = vk.name;
				vk.hatDir = SDL_HAT_RIGHTUP;
				virtualKeys.push_back(vk);

				vk.niceName = str.str() + "E";
				vk.name = "Joystick" + vk.niceName;
				vk.id = vk.name;
				vk.hatDir = SDL_HAT_RIGHT;
				virtualKeys.push_back(vk);

				vk.niceName = str.str() + "SE";
				vk.name = "Joystick" + vk.niceName;
				vk.id = vk.name;
				vk.hatDir = SDL_HAT_RIGHTDOWN;
				virtualKeys.push_back(vk);

				vk.niceName = str.str() + "S";
				vk.name = "Joystick" + vk.niceName;
				vk.id = vk.name;
				vk.hatDir = SDL_HAT_DOWN;
				virtualKeys.push_back(vk);

				vk.niceName = str.str() + "SW";
				vk.name = "Joystick" + vk.niceName;
				vk.id = vk.name;
				vk.hatDir = SDL_HAT_LEFTDOWN;
				virtualKeys.push_back(vk);

				vk.niceName = str.str() + "W";
				vk.name = "Joystick" + vk.niceName;
				vk.id = vk.name;
				vk.hatDir = SDL_HAT_LEFT;
				virtualKeys.push_back(vk);

				vk.niceName = str.str() + "NW";
				vk.name = "Joystick" + vk.niceName;
				vk.id = vk.name;
				vk.hatDir = SDL_HAT_LEFTUP;
				virtualKeys.push_back(vk);
			}

			joysticks.push_back(p_joystick);
			index++;
		}
	}

	resolveActionKeys();
	limitActionKeys();
	repairLostBindings();

	// Only now: getDesktopSize() needs SDL, and after the first
	// SDL_SetVideoMode, SDL_GetVideoInfo reports the window size rather than
	// the desktop size.
	if(windowedSize.x <= 0 || windowedSize.y <= 0) windowedSize = getDefaultWindowSize();
	// A window that no longer fits on the screen is cut back to size.
	const Vec2i desktop = getDesktopSize();
	if(windowedSize.x > desktop.x || windowedSize.y > desktop.y)
		windowedSize = getDefaultWindowSize();

	char videoDriver[256] = "";
	SDL_VideoDriverName(videoDriver, 256);
	printfLog("  Video driver: %s\n", videoDriver);

	// initialize OpenGL
	printfLog("* Initializing OpenGL ...\n");

	SDL_GL_SetAttribute(SDL_GL_BUFFER_SIZE, 32);
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 1);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	if(!windowIconFilename.empty())
	{
		// load the icon
		FileSystem& fs = FileSystem::inst();
		File* p_file = fs.openFile(windowIconFilename);
		SDL_RWops* p_rwOps = p_file->getRWOps();
		SDL_Surface* p_surface = IMG_Load_RW(p_rwOps, 1);
		SDL_Surface* p_rgba = SDL_CreateRGBSurface(SDL_SWSURFACE, p_surface->w, p_surface->h, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
		SDL_SetAlpha(p_surface, 0, 0);
		SDL_BlitSurface(p_surface, 0, p_rgba, 0);
		SDL_FreeSurface(p_surface);

		// build the mask
		SDL_LockSurface(p_rgba);
		Uint8* p_mask = new Uint8[(p_rgba->w + 7) / 8 * p_rgba->h];
		uint cursor = 0;
		for(int y = 0; y < p_rgba->h; y++)
		{
			Uint8 byte = 0;
			for(int x = 0; x < p_rgba->w; x++)
			{
				// get the pixel colour
				uint rgba = reinterpret_cast<uint*>(p_rgba->pixels)[y * (p_rgba->pitch / 4) + x];

				// extract the alpha value
				rgba &= p_rgba->format->Amask;
				rgba >>= p_rgba->format->Ashift;

				// set the bit, or not
				byte <<= 1;
				if(rgba >= 127) byte |= 1;

				if(!((x + 1) % 8) || x == p_rgba->w - 1)
				{
					// write the finished byte
					p_mask[cursor++] = byte;
					byte = 0;
				}
			}
		}

		SDL_UnlockSurface(p_rgba);

		SDL_WM_SetIcon(p_rgba, p_mask);
		SDL_FreeSurface(p_rgba);
		delete[] p_mask;
	}

	// SDL's flags stay unchanged from here on - SDL_OPENGL | SDL_RESIZABLE, for
	// the whole life of the process. Only then does DIB_SetVideoMode hit its
	// fast path, and the GL context survives every resize.
	displaySize = windowedSize;
	p_display = SDL_SetVideoMode(displaySize.x, displaySize.y, 32, SDL_OPENGL | SDL_RESIZABLE);
	if(!p_display)
	{
		printfLog("+ ERROR: Could not set video mode (Error: %s).\n", SDL_GetError());
		return false;
	}

	// Back to where it last stood.
	restoreWindowPosition();

#ifdef _WIN32
	// The window stands from here; our own window procedure can go in front.
	hookWindowProc();
#endif

#ifdef __EMSCRIPTEN__
	// Only a real key press may trigger the Fullscreen API, hence at the DOM.
	emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, 0, EM_TRUE,
									engineFullScreenHotkey);
	// And on a phone the same for the finger: there the game takes the
	// fullscreen itself, see enforceTouchFullScreen().
	//
	// Both ends of the touch, and that is not belt and braces: the Fullscreen
	// API demands a *transient* user activation, and a phone does not
	// necessarily grant one as early as the finger going down - touchend is
	// the event the specification names for it. Emscripten's own
	// emscripten_request_fullscreen_strategy defers the request in exactly
	// this case to the next event allowed to perform it, and a plain
	// requestFullscreen() has no such second chance. Both are therefore
	// registered; whichever is allowed first wins, and the second call finds
	// the fullscreen already standing.
	emscripten_set_touchstart_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, 0, EM_TRUE,
									   engineTouchFullScreen);
	emscripten_set_touchend_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, 0, EM_TRUE,
									 engineTouchFullScreen);
#endif

	SDL_ShowCursor(0);

	const char* p_vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
	const char* p_renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
	const char* p_version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
	int buffer, red, green, blue, alpha, depth, stencil, dbuffer;
	SDL_GL_GetAttribute(SDL_GL_BUFFER_SIZE, &buffer);
	SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &red);
	SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &green);
	SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &blue);
	SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE, &alpha);
	SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &depth);
	SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &stencil);
	SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER, &dbuffer);

	printfLog("  ============================================================\n");
	printfLog("  Vendor:           %s\n", p_vendor);
	printfLog("  Renderer:         %s\n", p_renderer);
	printfLog("  Version:          %s\n", p_version);
	printfLog("  Resolution:       %dx%d\n", displaySize.x, displaySize.y);
	printfLog("  Bits per pixel:   %d (R=%d, G=%d, B=%d, A=%d)\n", buffer, red, green, blue, alpha);
	printfLog("  Depth bits:       %d\n", depth);
	printfLog("  Stencil bits:     %d\n", stencil);
	printfLog("  Double buffering: %s\n", dbuffer ? "On" : "Off");
	printfLog("  SDL display:      Flags=%x, BPP=%d, Masks=(%x, %x, %x, %x)\n", p_display->flags, p_display->format->BitsPerPixel, p_display->format->Rmask, p_display->format->Gmask, p_display->format->Bmask, p_display->format->Amask);
	printfLog("  ============================================================\n");

#ifdef __EMSCRIPTEN__
	// In GLES2/WebGL glBlendFuncSeparate is core, but the extension is not
	// advertised - the query below could never find it.
	glExtBlendFuncSeparate = reinterpret_cast<PFNGLBLENDFUNCSEPARATEEXTPROC>(&glBlendFuncSeparate);
	printfLog("  Separate blending is core in WebGL; using it directly.\n");
	printfLog("  ============================================================\n");
#endif

	// query the extensions
	const char* p_extensions = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
	if(strstr(p_extensions, "GL_EXT_blend_func_separate"))
	{
		void* p_proc = SDL_GL_GetProcAddress("glBlendFuncSeparate");
		if(p_proc)
		{
			printfLog("  Extension GL_EXT_blend_func_separate is available.\n");
			printfLog("  ============================================================\n");
			glExtBlendFuncSeparate = reinterpret_cast<PFNGLBLENDFUNCSEPARATEEXTPROC>(p_proc);
		}
	}

	// Create the framebuffer object. If that fails, rendering goes straight
	// into the back buffer.
	GLExtensions::init();
	useFrameBuffer = createFrameBuffer();
	if(!useFrameBuffer)
	{
		printfLog("- WARNING: No framebuffer object; rendering straight to the back buffer.\n");

		// Then it stays at 640x480, see handleResize(): a size taken from
		// config.xml has to go back, and there is no fullscreen, because a
		// screen-filling window would put the picture in a corner.
		fullScreen = false;
		handleResize(screenSize.x, screenSize.y);
		fixWindowSize();
	}
	else if(GLExtensions::haveShaders())
	{
		createUpscalerGL();
	}

	// If the game starts in fullscreen, the style change comes now - only here,
	// because handleResize() has to know the framebuffer.
	if(fullScreen) applyWindowStyle(true, getDesktopSize());
	{
		std::string available;
		for(std::vector<Upscaler*>::const_iterator i = upscalers.begin(); i != upscalers.end(); ++i)
		{
			if(!(*i)->isAvailable()) continue;
			if(!available.empty()) available += ", ";
			available += (*i)->getName();
		}
		printfLog("  Upscale filters:  %s\n", available.c_str());
	}
	printfLog("  Upscaling:        %s\n", getEffectiveUpscaler()->getName());

	// Only here: how large the cursor must be hangs off the framebuffer.
	setupCursor();

	// create the textures for crossfading
	glGenTextures(1, &oldImageID);
	glGenTextures(1, &newImageID);
	glBindTexture(GL_TEXTURE_2D, oldImageID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, screenPow2Size.x, screenPow2Size.y, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, newImageID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, screenPow2Size.x, screenPow2Size.y, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// initialize OpenAL
	printfLog("* Initializing OpenAL ...\n");

	std::string bestDevice = getBestOpenALDevice();
	if(bestDevice == "[NONE]")
	{
		printfLog("+ ERROR: Please install current version of OpenAL and audio drivers.\n");
		return false;
	}

	printfLog("  ============================================================\n");
	printfLog("  Selected output:  %s\n", bestDevice.c_str());
	printfLog("  ============================================================\n");

	p_audioDevice = alcOpenDevice(bestDevice.c_str());
	if(!p_audioDevice)
	{
		printfLog("+ ERROR: Could not open audio device.\n");
		return false;
	}

	// OpenAL can only record input devices, that is the microphone. What is
	// needed is the output - hence WASAPI's loopback mode.
	p_audioCapture = new AudioCapture;
	if(p_audioCapture->open(48000))
	{
		printfLog("  Recording audio from: %s (loopback)\n", p_audioCapture->getDeviceName().c_str());
		printfLog("  ============================================================\n");
	}
	else
	{
		delete p_audioCapture;
		p_audioCapture = 0;
		printfLog("+ WARNING: Could not open audio capture device. Captured videos will be without audio!\n");
	}

	p_audioContext = alcCreateContext(p_audioDevice, 0);
	if(!p_audioContext)
	{
		printfLog("+ ERROR: Could not create audio context (Error: %d).\n", alcGetError(p_audioDevice));
		return false;
	}

	if(!alcMakeContextCurrent(p_audioContext))
	{
		printfLog("+ ERROR: Could not activate audio context (Error: %d).\n", alcGetError(p_audioDevice));
		return false;
	}

	alcProcessContext(p_audioContext);

#ifdef __EMSCRIPTEN__
	// Emscripten's OpenAL turns queued buffers into Web Audio nodes from a
	// setInterval on the main thread, and schedules only 0.1 s ahead. A frame
	// longer than that leaves the streamed music with nothing scheduled and
	// tears a hole in it - so the gaps arrive at the frame rate rather than on
	// the quarter-second buffer boundaries, which is what tells this apart
	// from a queue that is simply not being refilled.
	//
	// Half a second costs two more AudioBufferSourceNodes on the one streaming
	// source there is, and no latency anywhere: stopping, pausing and
	// restarting all go through stopSourceAudio(), which stops every scheduled
	// node outright. Past one second there would be nothing left to schedule -
	// that is all the audio the four queued buffers hold. The effects need
	// none of it either way, since a looping one is a single node with
	// loop = true and a one-shot is its whole buffer in one node, and neither
	// is ever rescheduled.
	//
	// QUEUE_LOOKAHEAD belongs to Emscripten - read out of emsdk 6.0.8 - and an
	// upgrade may move it, so the value is read back rather than assumed and
	// the log line is what says whether it took.
	const double WEB_AUDIO_LOOKAHEAD = 0.5;
	const double lookahead = EM_ASM_DOUBLE(
	{
		if(typeof AL === 'undefined' || typeof AL.QUEUE_LOOKAHEAD !== 'number') return -1.0;
		AL.QUEUE_LOOKAHEAD = $0;
		return AL.QUEUE_LOOKAHEAD;
	}, WEB_AUDIO_LOOKAHEAD);

	if(lookahead < 0.0) printfLog("+ WARNING: No AL.QUEUE_LOOKAHEAD to raise - the music will break up on a long frame.\n");
	else printfLog("  Web Audio lookahead: %.0f ms\n", lookahead * 1000.0);
#endif

	// Headroom for the mix. The individual sources stay as they are - only the
	// finished mix gets quieter, and it does that before OpenAL Soft clamps it
	// to [-1, 1].
	alListenerf(AL_GAIN, static_cast<float>(MASTER_HEADROOM));

	printfLog("* Initializing GUI ...\n");
	if(!GUI::inst().init())
	{
		printfLog("+ ERROR: Could not initialize GUI.\n");
		return false;
	}

	// set the OpenGL settings
	glViewport(0, 0, width, height);
#ifndef __EMSCRIPTEN__
	// GL_SMOOTH is the default; Emscripten's GL reimplementation aborts on it.
	glShadeModel(GL_SMOOTH);
#endif
	glEnable(GL_BLEND);
	glEnable(GL_POINT_SMOOTH);
#ifndef __EMSCRIPTEN__
	// Neither hint target exists in WebGL (INVALID_ENUM).
	glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
#endif

	setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
	if(!glExtBlendFuncSeparate) GUI::inst().setOpacity(1.0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	// pixel screen coordinates
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluOrtho2D(0.0, width, height, 0.0);

	glMatrixMode(GL_MODELVIEW);

	setLogicRate(20);

	p_stateToBeEntered = p_stateToGetFocus = p_stateToLoseFocus = 0;
	p_currentMusic = 0;
	currentMusicFilename = "";
	volumeChanged = false;

	FileSystem& fs = FileSystem::inst();
	const std::string timePlayedStr = fs.fileExists(fs.getAppHomeDirectory() + ".time_played") ? fs.readStringFromFile(fs.getAppHomeDirectory() + ".time_played") : "";
	if(!timePlayedStr.empty()) timePlayed = static_cast<uint>(atoi(timePlayedStr.c_str()));

	initialized = true;

	return true;
}

void Engine::exit()
{
	if(!initialized) return;

	// Window size, position and fullscreen state are to survive to the next
	// start, even if the player never opened the options dialog.
	rememberWindowPlacement();
	saveConfig();

	if(p_videoRecorder)
	{
		// stop the recording
		delete p_videoRecorder;
		p_videoRecorder = 0;
	}

	// leave the current game state
	setGameState("");

	// shut down the GUI
	printfLog("* Shutting down GUI ...\n");
	GUI::inst().exit();

#ifdef _WIN32
	// Our own window procedure back out before SDL tears the window down.
	unhookWindowProc();
#endif

	// release the framebuffer while the GL context still stands
	destroyUpscalerGL();
	destroyFrameBuffer();

	// shut down the managers
	printfLog("* Shutting down resource managers ...\n");
	Manager<TileSet>::inst().exit();
	Manager<Font>::inst().exit();
	Manager<Texture>::inst().exit();
	Manager<Sound>::inst().exit();
	Manager<StreamedSound>::inst().exit();

	// shut down OpenAL
	printfLog("* Shutting down OpenAL ...\n");
	alcSuspendContext(p_audioContext);
	alcMakeContextCurrent(0);
	alcDestroyContext(p_audioContext);
	alcCloseDevice(p_audioDevice);
	delete p_audioCapture;
	p_audioCapture = 0;

	// delete the crossfade and the textures
	crossfade(0, 0.0);
	glDeleteTextures(1, &oldImageID);
	glDeleteTextures(1, &newImageID);

	// close the joysticks
	for(std::vector<SDL_Joystick*>::const_iterator it = joysticks.begin();
		it != joysticks.end();
		++it)
	{
		SDL_JoystickClose(*it);
	}

	joysticks.clear();

	// shut down SDL
	printfLog("* Shutting down SDL ...\n");
	SDL_Cursor* p_cursor = SDL_GetCursor();
	SDL_FreeCursor(p_cursor);
	SDL_Quit();

	// delete the actions
	for(size_t i = 0; i < actionsVector.size(); i++) delete actionsVector[i];
	actionsVector.clear();
	actions.clear();

	saveTimePlayed();

	initialized = false;
}

// #define RECORD
// #define PROFILE_VIDEO_CAPTURE

#ifdef __EMSCRIPTEN__
// In the browser emscripten_set_main_loop calls one pass per frame, letting
// the page draw in between - which puts the loop state here.
namespace
{
	bool   done = false;
	Uint32 timeToProcess = 0;
	uint   timeProcessed = 1;
	uint   firstEventRecorded = ~0u;
}

static void emMainLoopIteration(void* p_engine);
#endif

void Engine::handleAppFocus(bool gained)
{
	if(gained == appActive) return;
	appActive = gained;

	if(gained)
	{
		if(oldSoundVolume != -1.0)
		{
			setSoundVolume(oldSoundVolume);
			setMusicVolume(oldMusicVolume);
			oldSoundVolume = -1.0;
		}

		GameState* p_gs = getGameState();
		if(p_gs) p_gs->onAppGetFocus();
		return;
	}

	oldSoundVolume = soundVolume;
	oldMusicVolume = musicVolume;
	setSoundVolume(0.0);
	setMusicVolume(0.0);

	// No release arrives after a change of focus. A key left standing as held
	// here would never yield a key press again.
	for(int i = 0; i < NUM_KEY_SLOTS; i++) keyHeld[i] = false;

	GameState* p_gs = getGameState();
	if(p_gs) p_gs->onAppLoseFocus();

	// stop the video recording if one is running
	if(p_videoRecorder)
	{
		delete p_videoRecorder;
		p_videoRecorder = 0;
	}
}

void Engine::mainLoop()
{
#ifndef __EMSCRIPTEN__
	bool done = false;
	Uint32 timeToProcess = 0;
	uint timeProcessed = 1;
#ifdef RECORD
	// Only the demo recorder needs this. In the browser it lives in the
	// namespace above, because the loop saves nothing across a frame.
	uint firstEventRecorded = ~0u;
#endif
#endif

	// query the cursor position
	SDL_GetMouseState(&cursorPosition.x, &cursorPosition.y);

	processGameStateChanges();

#ifdef RECORD
	FILE* p_out = fopen("keyboard.dat", "wb");
#endif

#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop_arg(emMainLoopIteration, this, 0, 1);
}

void Engine::mainLoopIteration()
{
	// do/while(0), which keeps continue and break in the body meaning "end this
	// frame".
	do
	{
#else
	do
	{
#endif
		Uint32 start = SDL_GetTicks();

		// What this turn of the loop costs, in milliseconds. RENDER and
		// PRESENT are what *issuing* the draw calls costs; GL is asynchronous
		// and the drawing itself is paid for wherever the pipeline is next
		// made to catch up, which natively is the flush inside
		// glXSwapBuffers - hence SWAP as a phase of its own. In the browser
		// the swap does nothing at all and the compositing happens after this
		// function returns, so nothing here sees the GPU. See framestats.h.
		const double frameBegin = getExactTime();
		float phases[FrameStats::FS_NUM_PHASES];
		for(int i = 0; i < FrameStats::FS_NUM_PHASES; i++) phases[i] = 0.0f;

#ifdef __EMSCRIPTEN__
		// SDL 1.2 makes no SDL_VIDEORESIZE out of a change of canvas size;
		// looking once per frame catches both the window and the API.
		{
			int canvasWidth = 0, canvasHeight = 0;
			emscripten_get_canvas_element_size("#canvas", &canvasWidth, &canvasHeight);
			if(canvasWidth > 0 && canvasHeight > 0 &&
			   (canvasWidth != displaySize.x || canvasHeight != displaySize.y))
				handleResize(canvasWidth, canvasHeight);
		}
#endif

		// has an OpenGL error occurred?
		uint err = glGetError();
		if(err != GL_NO_ERROR)
		{
			printfLog("+ An OpenGL error occured (Error: %d).\n", err);
		}

		err = alGetError();
		if(err != AL_NO_ERROR)
		{
			printfLog("+ An OpenAL error occured (Error: %d).\n", err);
		}

		bool frameRendered = false;

		// render
		if(appActive && timeProcessed)
		{
			const double renderBegin = getExactTime();
			bindFrameBuffer();
			render();
			phases[FrameStats::FS_RENDER] = static_cast<float>((getExactTime() - renderBegin) * 1000.0);
			frameRendered = true;
		}

		// process the SDL events
		SDL_Event event;
		while(SDL_PollEvent(&event))
		{
			switch(event.type)
			{
			case SDL_ACTIVEEVENT:
				if(event.active.state & SDL_APPACTIVE || event.active.state & SDL_APPINPUTFOCUS)
					handleAppFocus(event.active.gain != 0);
				break;
#ifdef __EMSCRIPTEN__
			// Emscripten's SDL reports focus and visibility as
			// SDL_WINDOWEVENT - an SDL 2 shape - and never sends an
			// SDL_ACTIVEEVENT. Without this branch the game in the browser
			// learns nothing of it and would simply keep running in the
			// background instead of halting as it does everywhere else.
			case SDL_WINDOWEVENT:
				switch(event.window.event)
				{
				case SDL_WINDOWEVENT_FOCUS_GAINED:
				case SDL_WINDOWEVENT_SHOWN:
					handleAppFocus(true);
					break;
				case SDL_WINDOWEVENT_FOCUS_LOST:
				case SDL_WINDOWEVENT_HIDDEN:
					handleAppFocus(false);
					break;
				default:
					break;
				}
				break;
#endif
			// A block of its own: a variable created inside a case would
			// otherwise not be allowed to live across the next case label.
			case SDL_KEYDOWN:
				{
				// Is this the repeat of a held key?
				// SDL_EnableKeyRepeat(140, 60) keeps sending further
				// SDL_KEYDOWN for a key nobody released, and those are not a
				// new key press. This stands before the two key combinations
				// below, because those are commands too: a held Alt+Return
				// would otherwise toggle the fullscreen every 60 ms.
				//
				// keyHeld is set here already, keyData only further down -
				// the two combinations swallow their key, and swallowed
				// means this too: wasKeyPressed() does not see it.
				const int keySlot = event.key.keysym.sym;
				const bool inRange = keySlot >= 0 && keySlot < NUM_KEY_SLOTS;
				const bool repeat = inRange && keyHeld[keySlot];
				if(inRange) keyHeld[keySlot] = true;

#ifndef __EMSCRIPTEN__
				// Alt+F4 has to quit the game. SDL's windib window procedure
				// treats WM_SYSKEYDOWN as an ordinary key press and returns 0;
				// DefWindowProc never sees it.
				if(event.key.keysym.sym == SDLK_F4 &&
				   (event.key.keysym.mod & KMOD_ALT || SDL_GetModState() & KMOD_ALT))
				{
					if(!repeat)
					{
						SDL_Event quitEvent;
						quitEvent.type = SDL_QUIT;
						SDL_PushEvent(&quitEvent);
					}
					break;
				}
#endif
				// Alt+Return toggles the fullscreen and is swallowed; the
				// game must never see a bare Return in it. The keypad's Enter
				// counts, because a keyboard has two of these keys and nothing
				// here tells them apart.
				if(isReturnKey(event.key.keysym.sym) &&
				   (event.key.keysym.mod & KMOD_ALT || SDL_GetModState() & KMOD_ALT))
				{
					swallowedReturn = true;
#ifndef __EMSCRIPTEN__
					if(!repeat) toggleFullScreen();
#endif
					break;
				}

				if(inRange)
				{
					// The press bit only on the first press: otherwise
					// wasKeyPressed() would report a new press every 60 ms,
					// and an Escape held for a fifth of a second would close
					// the options dialog and quit the game right after -
					// consumeKeyPress() covers only the same tick, the
					// repeat comes in a later one.
					if(!repeat) keyData[keySlot] |= 2;
					keyData[keySlot] |= 1;
				}
				// The repeat itself stays: it goes to the GUI through this
				// queue, and a text field wants it. Anything that does not
				// want it can tell from the flag.
				{
					QueuedKeyEvent queued = { event.key, repeat };
					keyEventQueue.push(queued);
				}
				}
				break;
			case SDL_KEYUP:
				// keyHeld describes the keyboard and not the command, hence
				// before every special case: the swallowed Alt+Return jumps
				// straight out, and the key would otherwise stand as held
				// for ever.
				if(event.key.keysym.sym >= 0 && event.key.keysym.sym < NUM_KEY_SLOTS)
					keyHeld[event.key.keysym.sym] = false;

				// Do not hang it off the modifier: releasing Alt before Return
				// would otherwise leave a release without a press.
				if(isReturnKey(event.key.keysym.sym) && swallowedReturn)
				{
					swallowedReturn = false;
					break;
				}
				if(event.key.keysym.sym >= 0 && event.key.keysym.sym < NUM_KEY_SLOTS)
				{
					keyData[event.key.keysym.sym] &= ~1;
					keyData[event.key.keysym.sym] |= 4;
				}
				{
					QueuedKeyEvent queued = { event.key, false };
					keyEventQueue.push(queued);
				}
				break;
			case SDL_MOUSEBUTTONDOWN:
				// The position here too and not only on SDL_MOUSEMOTION: a
				// finger produces no motion at all. Emscripten's SDL makes an
				// SDL_MOUSEBUTTONDOWN out of touchstart and writes the spot
				// into it; without that the cursor on a tap would never land
				// where the finger did.
				cursorPosition = Vec2i(event.button.x, event.button.y);
				if(event.button.button < NUM_KEY_SLOTS)
					buttonData[event.button.button] |= (1 | 2);
				break;
			case SDL_MOUSEBUTTONUP:
				cursorPosition = Vec2i(event.button.x, event.button.y);
				if(event.button.button < NUM_KEY_SLOTS)
				{
					buttonData[event.button.button] &= ~1;
					buttonData[event.button.button] |= 4;
				}
				break;
			case SDL_MOUSEMOTION:
				cursorPosition = Vec2i(event.motion.x, event.motion.y);
				break;
			case SDL_VIDEORESIZE:
				// Comes from dragging the window border as well as from the
				// style change in applyWindowStyle(): one path for both.
				handleResize(event.resize.w, event.resize.h);
				break;
			case SDL_QUIT:
#ifdef __EMSCRIPTEN__
				// In the browser a program cannot close itself; the blue
				// screen halts the main loop as well.
				WebBlueScreen::show();
#else
				done = true;
#endif
				break;
			}
		}

		if(!appActive)
		{
			// Do not compute, do not draw - but keep presenting. A window
			// that puts nothing up any more shows whatever Windows last had
			// of it, and that can be seconds old.
			if(useFrameBuffer) showLastFrame();
			else if(!fullScreen) SDL_GL_SwapBuffers();

			updateSounds();
			SDL_Delay(50);
			continue;
		}

#ifdef RECORD
		if(wasKeyPressed(SDLK_HOME))
		{
			fclose(p_out);
			p_out = fopen("keyboard.dat", "wb");
			firstEventRecorded = ~0;
		}
#endif

		// move
		timeProcessed = 0;
		const double updateBegin = getExactTime();
		while(timeToProcess >= logicRate)
		{
			update();

#ifdef RECORD
			bool output = false;
			for(int i = 0; i < NUM_KEY_SLOTS; i++)
			{
				if(keyData[i])
				{
					output = true;
					break;
				}
			}

			if(output)
			{
				if(firstEventRecorded == ~0) firstEventRecorded = time;
				uint t = time - firstEventRecorded;
				fwrite(&t, 4, 1, p_out);
			}
#endif

			// reset the keyboard and mouse data
			for(int i = 0; i < NUM_KEY_SLOTS; i++)
			{
#ifdef RECORD
				if(output && keyData[i])
				{
					fwrite(&i, 4, 1, p_out);
					fwrite(&keyData[i], 4, 1, p_out);
				}
#endif

				keyData[i] &= ~(2 | 4);
				buttonData[i] &= ~(2 | 4);
			}

			// One queue, once - not once per key slot, which is what it read
			// as with the closing brace one line further down.
			while(!keyEventQueue.empty()) keyEventQueue.pop();

			// reset the action data
			clearActionEdges();

#ifdef RECORD
			if(output)
			{
				uint end = ~0;
				fwrite(&end, 4, 1, p_out);
			}
#endif

			timeToProcess -= logicRate;
			timeProcessed += logicRate;
			time += logicRate;
		}

		phases[FrameStats::FS_UPDATE] = static_cast<float>((getExactTime() - updateBegin) * 1000.0);

		if(crossfadeTime == -0.51)
		{
			// save the old image. The framebuffer has to be bound explicitly
			// for that: without a logic tick nothing is rendered, and then the
			// screen is still bound - which WebGL clears before every frame.
			bindFrameBuffer();
			glBindTexture(GL_TEXTURE_2D, oldImageID);
			glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, screenPow2Size.y - screenSize.y, 0, 0, screenSize.x, screenSize.y);
			crossfadeTime = -0.5;
		}
		else if(crossfadeTime >= -0.5 && frameRendered)
		{
			// fetch the current image
			glBindTexture(GL_TEXTURE_2D, newImageID);
			glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, screenPow2Size.y - screenSize.y, 0, 0, screenSize.x, screenSize.y);

			// render the crossfade
			p_crossfade->render(max(0.0, crossfadeTime / crossfadeDuration), oldImageID, newImageID);
		}

		if(timeProcessed)
		{
			// update the crossfade
			if(crossfadeTime >= 0.0)
			{
				crossfadeTime += 0.001 * timeProcessed;
				if(crossfadeTime > crossfadeDuration)
				{
					// The crossfade is over!
					crossfadeTime = -1.0;
					crossfadeDuration = 0.0;
					delete p_crossfade;
					p_crossfade = 0;
				}
			}
			else if(crossfadeTime == -0.5) crossfadeTime = -0.25;
			else if(crossfadeTime == -0.25) crossfadeTime = 0.0;
		}

		if(frameRendered)
		{
			if(p_videoRecorder && p_videoRecorder->isReadyForNextFrame())
			{
				// record a new frame?
				const uint timecode = static_cast<uint>((getExactTimeMS() - recordingStartTime) / (1000.0 / p_videoRecorder->getFPS()));
				if(timecode != lastRecordedFrameTimecode)
				{
					void* p_inputFrameBuffer = p_videoRecorder->getInputFrameBuffer();

#ifdef PROFILE_VIDEO_CAPTURE
					BEGIN_PROFILE(videoCapture)
#endif

					// Fetch the frame. Always 640x480 out of the framebuffer, whatever
					// the window size - the video encoder is set up for that once.
					glReadBuffer(useFrameBuffer ? GL_COLOR_ATTACHMENT0_EXT : GL_BACK);
					glReadPixels(0, 0, screenSize.x, screenSize.y, GL_RGBA, GL_UNSIGNED_BYTE, p_inputFrameBuffer);

					if(SDL_ShowCursor(-1))
					{
						// Draw the mouse cursor into the buffer by hand. Always
						// at its designed size: what gets recorded is the
						// 640x480 frame whatever the window is doing, and in
						// that the cursor is 16x16. What the system draws on
						// the screen beside it can be twice as large - see
						// updateCursorSize().
						const Vec2i cursorPosition(getCursorPosition());
						for(int dy = 0; dy < 16 && cursorPosition.y + dy < screenSize.y; ++dy)
						{
							for(int dx = 0; dx < 16 && cursorPosition.x + dx < screenSize.x; ++dx)
							{
								const int color = cursorImage[dy][dx];
								if(color != -1)
								{
									const Vec2i pixelPosition(cursorPosition + Vec2i(dx, dy));
									reinterpret_cast<uint32_t*>(p_inputFrameBuffer)[(screenSize.y - 1 - pixelPosition.y) * screenSize.x + pixelPosition.x] = color ? 0xFFFFFFFF : 0x00000000;
								}
							}
						}
					}

#ifdef PROFILE_VIDEO_CAPTURE
					END_PROFILE(videoCapture)
#endif

					p_videoRecorder->encodeNextFrame(timecode);
					lastRecordedFrameTimecode = timecode;
				}
			}

			drawOverlays();

			// Before presenting, so the screenshot captures the clean 640x480
			// frame and not the scaled version with its black bars.
			if(doScreenshot)
			{
				doScreenshot = false;
				// The sound confirms the picture; with no picture it
				// confirms nothing.
				if(screenshot()) playSound("screenshot.ogg");
			}

			// put the framebuffer on the screen
			const double presentBegin = getExactTime();
			unbindFrameBuffer();
			presentFrame();
			const double swapBegin = getExactTime();
			phases[FrameStats::FS_PRESENT] = static_cast<float>((swapBegin - presentBegin) * 1000.0);

			// show the rendered frame
			SDL_GL_SwapBuffers();
			phases[FrameStats::FS_SWAP] = static_cast<float>((getExactTime() - swapBegin) * 1000.0);
		}

		Uint32 end = SDL_GetTicks();
		if(frameRendered) frameTime = end - start;

		// TOTAL stops here and not after the SDL_Delay below: what is wanted
		// is the work, not the waiting. INTERVAL is start to start and so
		// carries the wait with it, which is what makes the two different
		// numbers worth having side by side.
		{
			const double frameEnd = getExactTime();
			phases[FrameStats::FS_TOTAL] = static_cast<float>((frameEnd - frameBegin) * 1000.0);
			if(lastFrameBegin > 0.0)
				phases[FrameStats::FS_INTERVAL] = static_cast<float>((frameBegin - lastFrameBegin) * 1000.0);
			lastFrameBegin = frameBegin;
			frameStats.addFrame(phases);
		}

		// wait if there is still enough time
		uint dt = end - start;
#ifdef __EMSCRIPTEN__
		// requestAnimationFrame sets the pace. The logic clock, though, has to
		// advance by the time between two callbacks, not by the time in one.
		{
			static Uint32 lastFrameEnd = 0;
			if(lastFrameEnd) dt = end - lastFrameEnd;
			lastFrameEnd = end;
		}
#else
		if(timeToProcess + dt < logicRate)
		{
			SDL_Delay(logicRate - (timeToProcess + dt));
			end = SDL_GetTicks();
			dt = end - start;
		}
#endif

		timeToProcess += dt;
		timeToProcess = min<uint>(250, timeToProcess);

#ifdef __EMSCRIPTEN__
	} while(0);
}

static void emMainLoopIteration(void* p_engine)
{
	static_cast<Engine*>(p_engine)->mainLoopIteration();
}
#else
	} while(!done);

#ifdef RECORD
	fclose(p_out);
#endif
}
#endif

namespace
{
	// Height of one toast bar. The stack moves in exactly these steps, and a
	// toast slides in and out again by exactly this much.
	const int TOAST_HEIGHT = 35;

	// How long the slide in and the slide out take. Both are added to the hold
	// time and not counted against it.
	const uint TOAST_FADE = 100;

	// Hold times where showToast names none. An error stands longer, because
	// it has to be read and usually acted on as well.
	const double TOAST_SECONDS_OK    = 2.0;
	const double TOAST_SECONDS_ERROR = 4.0;
}

void Engine::showToast(ToastType type,
					   const std::string& text,
					   double duration,
					   bool suppressSound)
{
	if(duration <= 0.0) duration = (type == TOAST_ERROR) ? TOAST_SECONDS_ERROR : TOAST_SECONDS_OK;
	const uint durationMS = static_cast<uint>(duration * 1000.0);

	// The sound hangs off the click and not off the message: it comes even
	// when the same message already stands and merely stays longer.
	if(type == TOAST_ERROR && !suppressSound) playSound("teleport_failed.ogg", false, 0.0, 100);

	// If the same message already stands, no second copy but the longer hold
	// time of the two. One that is already sliding out does not count.
	for(std::list<Toast>::iterator i = toasts.begin(); i != toasts.end(); ++i)
	{
		if(i->phase == 2 || i->type != type || i->text != text) continue;

		if(i->phase == 0)
		{
			// It is still sliding in, its hold time has not started yet.
			i->duration = max(i->duration, durationMS);
		}
		else
		{
			const uint left = i->duration > i->phaseTime ? i->duration - i->phaseTime : 0;
			if(durationMS > left) i->duration = i->phaseTime + durationMS;
		}

		return;
	}

	// A new toast comes in at the top and pushes the others down.
	Toast toast;
	toast.type = type;
	toast.text = text;
	toast.phase = 0;
	toast.phaseTime = 0;
	toast.duration = durationMS;
	toast.y = -static_cast<double>(TOAST_HEIGHT);
	toast.targetY = 0.0;
	toasts.push_back(toast);

	reflowToasts();
}

void Engine::reflowToasts()
{
	// Back to front: the newest toast gets the topmost slot. One that is
	// sliding out keeps its target one slot above.
	int slot = 0;
	for(std::list<Toast>::reverse_iterator i = toasts.rbegin(); i != toasts.rend(); ++i)
	{
		if(i->phase == 2) continue;
		i->targetY = static_cast<double>(slot * TOAST_HEIGHT);
		slot++;
	}
}

void Engine::updateToasts()
{
	if(toasts.empty()) return;

	// A toast gets this far in one tick: one bar height in the time of a fade.
	// A change of slot therefore takes as long as the slide in.
	const double step = static_cast<double>(TOAST_HEIGHT) * logicRate / TOAST_FADE;

	bool slotsFreed = false;

	for(std::list<Toast>::iterator i = toasts.begin(); i != toasts.end(); )
	{
		i->phaseTime += logicRate;

		if(i->phase == 0)
		{
			if(i->phaseTime >= TOAST_FADE)
			{
				i->phase = 1;
				i->phaseTime = 0;
			}
		}
		else if(i->phase == 1)
		{
			if(i->phaseTime >= i->duration)
			{
				// Out: one slot upward, hence either off the screen or behind
				// the toast above.
				i->phase = 2;
				i->phaseTime = 0;
				i->targetY -= TOAST_HEIGHT;
				slotsFreed = true;
			}
		}
		else if(i->phaseTime >= TOAST_FADE)
		{
			i = toasts.erase(i);
			continue;
		}

		if(i->y < i->targetY) i->y = min(i->y + step, i->targetY);
		else if(i->y > i->targetY) i->y = max(i->y - step, i->targetY);

		++i;
	}

	if(slotsFreed) reflowToasts();
}

void Engine::renderToasts()
{
	if(toasts.empty()) return;

	Font* p_font = GUI::inst().getFont();

	setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
	glLineWidth(1.0f);

	// Oldest first: the newer ones then lie on top, and a toast sliding out
	// disappears behind its younger neighbour.
	for(std::list<Toast>::const_iterator i = toasts.begin(); i != toasts.end(); ++i)
	{
		// Fading in and out goes together with the movement: the bar is not
		// fully opaque, and a sudden disappearance would show.
		double alpha = 1.0;
		if(i->phase == 0) alpha = static_cast<double>(i->phaseTime) / TOAST_FADE;
		else if(i->phase == 2) alpha = 1.0 - static_cast<double>(i->phaseTime) / TOAST_FADE;
		alpha = clamp(alpha, 0.0, 1.0);

		const Vec3d color = i->type == TOAST_ERROR ? Vec3d(0.5, 0.0, 0.0) : Vec3d(0.0, 0.5, 0.0);

		glPushMatrix();
		glTranslated(0.0, floor(i->y + 0.5), 0.0);

		glDisable(GL_TEXTURE_2D);
		glBegin(GL_QUADS);
		glColor4d(color.r, color.g, color.b, 0.75 * alpha);
		glVertex2i(0, 0);
		glVertex2i(640, 0);
		glColor4d(color.r, color.g, color.b, 0.9 * alpha);
		glVertex2i(640, TOAST_HEIGHT);
		glVertex2i(0, TOAST_HEIGHT);
		glEnd();
		glBegin(GL_LINES);
		glColor4d(0.0, 0.0, 0.0, 0.9 * alpha);
		glVertex2i(0, TOAST_HEIGHT);
		glVertex2i(640, TOAST_HEIGHT);
		glEnd();
		glEnable(GL_TEXTURE_2D);

		if(p_font) p_font->renderText(localizeString(i->text), Vec2i(10, 9), Vec4d(1.0, 1.0, 1.0, alpha));

		glPopMatrix();
	}
}

// #define PROFILE_ENGINE_RENDER

void Engine::render()
{
#ifdef PROFILE_ENGINE_RENDER
	BEGIN_PROFILE(engineRender)
#endif

	// Does the mouse cursor still match what is on the screen? Asked once a
	// frame rather than hung off events: the answer changes in more places
	// than one wants to remember - window size, fullscreen, a filter change,
	// the browser's canvas - and asking costs two divisions and a comparison.
	updateCursorSize();

	// An upper bound on what drawing costs, measured by not doing it: with
	// -perf on and "plant bomb" held, renderTiles(), renderSprite() and
	// Font::renderText() return at once, so the overlay reads what a frame
	// would cost if all three were free. A held button and not a switch,
	// because the phone is where the question matters and has no command
	// line; the on-screen pad's Bomb sends the Shift that action is bound to.
	// The statistics are cleared on each edge, or the percentile would mix
	// suppressed frames with ordinary ones for the twenty seconds the ring
	// holds at a phone's frame rate, and the button would look inert.
	const bool suppressWanted = performanceShown && isActionDown("$A_PLANT_BOMB");
	if(suppressWanted != renderSuppressWanted)
	{
		renderSuppressWanted = suppressWanted;
		frameStats.clear();
	}
	renderSuppressed = suppressWanted;

	// render the GUI
	GUI::inst().render();

	// render the game
	GameState* p_gs = getGameState();
	if(p_gs) p_gs->onRender();

	// display the GUI
	GUI::inst().display();

	// Toasts last: they sit over the GUI and over the editors' panes.
	renderToasts();

	// Off before drawOverlays(), which draws -perf's own numbers through the
	// same Font::renderText this suppresses. Without it the experiment would
	// hide its own answer.
	renderSuppressed = false;

#ifdef PROFILE_ENGINE_RENDER
	END_PROFILE(engineRender)
#endif
}

// #define PROFILE_ENGINE_UPDATE

void Engine::update()
{
#ifdef PROFILE_ENGINE_UPDATE
	BEGIN_PROFILE(engineUpdate)
#endif

#if defined(BLOCKS5_TEST_HOOKS) && !defined(__EMSCRIPTEN__)
	// Test build only. In the browser JavaScript calls the dump itself;
	// natively there is no such channel - see testhooks.cpp.
	TestHooks::pollRequests();
#endif

	// The on-screen pad labels its buttons with the names of the keys they
	// send, and those names are translated, so it has to be told which
	// language the game settled on. Asked here rather than pushed from the
	// places that assign it, because there are four of them - the detection,
	// the config, the options dialog and the string-table fallback - and a
	// fifth would be added one day without a call.
	publishLanguage();

	// update the virtual keys and actions
	updateVKs();

	// While a dialog is waiting for a key, this tick belongs to the key alone:
	// no actions and nothing for the GUI, where the cancelling Escape would go
	// on to close the dialog. The tick in which the key is found still counts.
	const bool grabbing = grabbingKey;
	if(grabbing)
	{
		updateKeyGrab();
		flushInput();
	}
	else updateActions();

	if(wasActionPressed("$A_CAPTURE_SCREENSHOT")) doScreenshot = true;

	if(wasActionPressed("$A_TOGGLE_MUTE"))
	{
		if(soundVolume == 0.0 && musicVolume == 0.0)
		{
			setSoundVolume(oldSoundVolume);
			setMusicVolume(oldMusicVolume);
			oldSoundVolume = oldMusicVolume = -1.0;
		}
		else
		{
			// mute
			oldSoundVolume = getSoundVolume();
			oldMusicVolume = getMusicVolume();
			setSoundVolume(0.0);
			setMusicVolume(0.0);
		}
	}

	if(wasActionPressed("$A_TOGGLE_CAPTURE_VIDEO"))
	{
		if(p_videoRecorder)
		{
			// stop the recording
			delete p_videoRecorder;
			p_videoRecorder = 0;
		}
		else
		{
			char videoDateTime[256];
			const time_t t = ::time(0);
			strftime(videoDateTime, 256, "%Y-%m-%d@%H-%M-%S", localtime(&t));
			const std::string filename(FileSystem::inst().getAppHomeDirectory() + "videos/" + videoDateTime + ".mp4");

			// start the recording
			p_videoRecorder = new VideoRecorder(filename, screenSize, screenSize, 2840000, p_audioCapture ? 160000 : 0, 30);
			if(p_videoRecorder->getError())
			{
				delete p_videoRecorder;
				p_videoRecorder = 0;
			}
			else
			{
				recordingStartTime = getExactTimeMS();
				lastRecordedFrameTimecode = ~0;
			}
		}
	}

#ifdef STRESS_TEST
	static int wurst = 0;
	if(!(wurst % 40))
	{
		const char* s[] = {"GS_LevelEditor", "GS_SelectLevel", "GS_CampaignEditor"};
		pushGameState(s[randomInt() % 3]);
	}
	else if(!((wurst + 20) % 40)) popGameState();
	wurst++;
#endif

	// update the GUI
	GUI::inst().update();

	processGameStateChanges();

	// update the game
	GameState* p_gs = getGameState();
	if(p_gs) p_gs->onUpdate();

	processGameStateChanges();

	updateToasts();

	updateSounds();

	++timePlayed;

	// Written down every 30 seconds. In the browser it is the only chance,
	// because emscripten_set_main_loop never returns and exit() never runs.
	if(!(timePlayed % 1500)) saveTimePlayed();

#ifdef PROFILE_ENGINE_UPDATE
	END_PROFILE(engineUpdate)
#endif
}

// Separate, because the browser has to write it as it goes rather than on
// quit - exit() never gets there.
void Engine::saveTimePlayed()
{
	std::ostringstream timePlayedStr;
	timePlayedStr << timePlayed;
	FileSystem::inst().writeStringToFile(timePlayedStr.str(),
										 FileSystem::inst().getAppHomeDirectory() + ".time_played");
}

void Engine::updateSounds()
{
	// update the sounds
	const std::unordered_multimap<std::string, Sound*>& sounds = Manager<Sound>::inst().getItems();
	for(std::unordered_multimap<std::string, Sound*>::const_iterator i = sounds.begin(); i != sounds.end(); ++i) i->second->update();

	// update the streamed sounds
	const std::unordered_multimap<std::string, StreamedSound*>& streamedSounds = Manager<StreamedSound>::inst().getItems();
	std::list<StreamedSound*> toBeDeleted;
	for(std::unordered_multimap<std::string, StreamedSound*>::const_iterator i = streamedSounds.begin(); i != streamedSounds.end(); ++i)
	{
		if(!i->second->update())
		{
			toBeDeleted.push_back(i->second);
		}
	}

	// release the stopped sounds
	for(std::list<StreamedSound*>::const_iterator i = toBeDeleted.begin(); i != toBeDeleted.end(); ++i) (*i)->release();

	if(volumeChanged) volumeChanged = false;
}

std::string Engine::getBestOpenALDevice()
{
	// take the default device
	const char* p_device = alcGetString(0, ALC_DEFAULT_DEVICE_SPECIFIER);
	if(!p_device) return "[NONE]";
	else return p_device;
}

void Engine::createUpscalerGL()
{
	if(shadersDisabled)
	{
		// No vertex buffer, no compiled programs: the filters then report
		// themselves as unavailable of their own accord, and
		// getEffectiveUpscaler() falls back to Sharp.
		printfLog("  Shaders:             switched off (-noshader)\n");
		return;
	}

	// WebGL forbids vertex data out of application memory, it has to be a
	// buffer. Four vertices, refilled every frame; every filter that uses a
	// shader shares this one.
	glExtGenBuffers(1, &presentVertexBuffer);
	if(!presentVertexBuffer)
	{
		// Without it no shader filter can draw. They are then simply left
		// uncompiled and report themselves as unavailable of their own accord -
		// no second condition is needed for that.
		printfLog("- WARNING: Could not create the present vertex buffer.\n");
		return;
	}

	// Each on its own: a CRT filter that does not compile is no reason to drop
	// SharpFit as well.
	for(std::vector<Upscaler*>::iterator i = upscalers.begin(); i != upscalers.end(); ++i)
	{
		if(!(*i)->createGL())
		{
			printfLog("- WARNING: The %s filter will not be available.\n", (*i)->getName());
		}
	}
}

void Engine::destroyUpscalerGL()
{
	for(std::vector<Upscaler*>::iterator i = upscalers.begin(); i != upscalers.end(); ++i)
	{
		(*i)->destroyGL();
	}
	if(presentVertexBuffer) { glExtDeleteBuffers(1, &presentVertexBuffer); presentVertexBuffer = 0; }
}

void Engine::setUpscaler(Upscaler* p_upscaler)
{
	// Only remembered. Whether the filter really works on this machine is
	// getEffectiveUpscaler()'s decision - there may be no GL context here yet.
	if(p_upscaler) p_wantedUpscaler = p_upscaler;
}

Upscaler* Engine::findUpscaler(const char* p_name) const
{
	if(!p_name) return 0;
	for(std::vector<Upscaler*>::const_iterator i = upscalers.begin(); i != upscalers.end(); ++i)
	{
		if(equalsNoCase(p_name, (*i)->getName())) return *i;
	}
	return 0;
}

Upscaler* Engine::getEffectiveUpscaler() const
{
	// Without a compiled program, sharp rather than no picture at all. The
	// wish stays as it is, for the next machine - nothing is rewritten here.
	//
	// The fallback is fixed Sharp and not "the first one that works": the
	// display order begins with SharpFit, and that belongs to the options
	// dialog. Otherwise its sorting would one day decide what a machine
	// without shaders shows.
	if(p_wantedUpscaler && p_wantedUpscaler->isAvailable()) return p_wantedUpscaler;
	return p_sharp;
}

bool Engine::createFrameBuffer()
{
	if(frameBufferDisabled)
	{
		printfLog("  Framebuffer objects: switched off (-nofbo)\n");
		return false;
	}

	if(!GLExtensions::haveFrameBufferObjects()) return false;

	frameTextureSize = screenPow2Size;

	glGenTextures(1, &frameTextureID);
	glBindTexture(GL_TEXTURE_2D, frameTextureID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frameTextureSize.x, frameTextureSize.y, 0,
				 GL_RGBA, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	glExtGenFramebuffers(1, &frameBufferID);
	glExtBindFramebuffer(GL_FRAMEBUFFER_EXT, frameBufferID);
	glExtFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
							  GL_TEXTURE_2D, frameTextureID, 0);

	// The star wipe (cf_star.cpp) and the light mask in level.cpp both need a
	// stencil buffer. A pure colour buffer would not be enough.
	glExtGenRenderbuffers(1, &frameDepthStencilID);
	glExtBindRenderbuffer(GL_RENDERBUFFER_EXT, frameDepthStencilID);
#ifdef __EMSCRIPTEN__
	// WebGL 1 knows exactly one combined format and one attachment point.
	glExtRenderbufferStorage(GL_RENDERBUFFER_EXT, GL_DEPTH_STENCIL_EXT,
							 frameTextureSize.x, frameTextureSize.y);
	glExtFramebufferRenderbuffer(GL_FRAMEBUFFER_EXT, GL_DEPTH_STENCIL_ATTACHMENT_EXT,
								 GL_RENDERBUFFER_EXT, frameDepthStencilID);
#else
	// EXT_packed_depth_stencil has no combined attachment point: the same
	// renderbuffer is attached to both, exactly as the specification requires.
	glExtRenderbufferStorage(GL_RENDERBUFFER_EXT, GL_DEPTH24_STENCIL8_EXT,
							 frameTextureSize.x, frameTextureSize.y);
	glExtFramebufferRenderbuffer(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT,
								 GL_RENDERBUFFER_EXT, frameDepthStencilID);
	glExtFramebufferRenderbuffer(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT,
								 GL_RENDERBUFFER_EXT, frameDepthStencilID);
#endif

	const GLenum status = glExtCheckFramebufferStatus(GL_FRAMEBUFFER_EXT);
	glExtBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);

	if(status != GL_FRAMEBUFFER_COMPLETE_EXT)
	{
		printfLog("- WARNING: Framebuffer object is incomplete (status 0x%x).\n", status);
		destroyFrameBuffer();
		return false;
	}

	printfLog("  Render target:    %dx%d in a %dx%d texture\n",
			  screenSize.x, screenSize.y, frameTextureSize.x, frameTextureSize.y);
	return true;
}

void Engine::destroyFrameBuffer()
{
	if(frameDepthStencilID)  { glExtDeleteRenderbuffers(1, &frameDepthStencilID); frameDepthStencilID = 0; }
	if(frameBufferID)        { glExtDeleteFramebuffers(1, &frameBufferID);        frameBufferID = 0; }
	if(frameTextureID)       { glDeleteTextures(1, &frameTextureID);              frameTextureID = 0; }
	if(renderTargetID)       { glExtDeleteFramebuffers(1, &renderTargetID);       renderTargetID = 0; }

	for(std::vector<OffscreenTexture>::const_iterator i = offscreenTextures.begin();
		i != offscreenTextures.end(); ++i)
	{
		glDeleteTextures(1, &i->id);
	}
	offscreenTextures.clear();
}

uint Engine::acquireOffscreenTexture(const Vec2i& size)
{
	if(!useFrameBuffer) return 0;

	// The miss path binds a texture raw, so it is a state change like any
	// other, and it happens before the caller reaches beginRenderToTexture -
	// whose flush would be one step too late.
	flushSprites();

	// One of the right size that nobody is holding?
	for(std::vector<OffscreenTexture>::iterator i = offscreenTextures.begin();
		i != offscreenTextures.end(); ++i)
	{
		if(!i->lent && i->size == size) { i->lent = true; return i->id; }
	}

	OffscreenTexture entry;
	entry.id = 0;
	entry.size = size;
	entry.lent = true;

	glGenTextures(1, &entry.id);
	if(!entry.id) return 0;

	glBindTexture(GL_TEXTURE_2D, entry.id);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.x, size.y, 0,
				 GL_RGBA, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	// WebGL 1 samples a texture whose edges are not a power of two as pure
	// black when it is repeated rather than clamped - with no error. This one
	// is a power of two, but clamped is right here anyway.
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	offscreenTextures.push_back(entry);
	return entry.id;
}

void Engine::releaseOffscreenTexture(uint textureID)
{
	// Only put back, not deleted: the next note gets the same one. After
	// destroyFrameBuffer() the list is empty and a late hand-back does nothing
	// - exactly right, because the texture is gone by then.
	for(std::vector<OffscreenTexture>::iterator i = offscreenTextures.begin();
		i != offscreenTextures.end(); ++i)
	{
		if(i->id == textureID) { i->lent = false; return; }
	}
}

bool Engine::beginRenderToTexture(uint textureID,
								  const Vec2i& size)
{
	// Both ends of the switch flush, which is what makes a bake inside an open
	// batch safe: quads queued before it belong on the screen, quads queued
	// during it belong on the texture, and each goes up where it was issued.
	flushSprites();

	if(!useFrameBuffer || !textureID) return false;

	if(!renderTargetID)
	{
		glExtGenFramebuffers(1, &renderTargetID);
		if(!renderTargetID) return false;
	}

	glExtBindFramebuffer(GL_FRAMEBUFFER_EXT, renderTargetID);
	glExtFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
							  GL_TEXTURE_2D, textureID, 0);
	if(glExtCheckFramebufferStatus(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT)
	{
		bindFrameBuffer();
		return false;
	}

	// A scissor box set elsewhere is in window coordinates and would clip the
	// texture here - the clear included.
	renderTargetScissor = (glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE);
	if(renderTargetScissor) glDisable(GL_SCISSOR_TEST);

	glViewport(0, 0, size.x, size.y);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	gluOrtho2D(0.0, size.x, size.y, 0.0);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	return true;
}

void Engine::endRenderToTexture()
{
	flushSprites();

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	// Detach the texture again: it is read in a moment, and a target that
	// doubles as a source is undefined.
	glExtFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
							  GL_TEXTURE_2D, 0, 0);
	if(renderTargetScissor) glEnable(GL_SCISSOR_TEST);
	bindFrameBuffer();
}

void Engine::bindFrameBuffer()
{
	if(!useFrameBuffer) return;
	glExtBindFramebuffer(GL_FRAMEBUFFER_EXT, frameBufferID);
	glViewport(0, 0, screenSize.x, screenSize.y);
}

void Engine::unbindFrameBuffer()
{
	if(!useFrameBuffer) return;
	glExtBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
	glViewport(0, 0, displaySize.x, displaySize.y);
}

#ifdef __EMSCRIPTEN__
// The browser grants fullscreen only on a real click or key press. SDL's
// events out of the animation loop do not count as one.
static EM_BOOL engineFullScreenHotkey(int, const EmscriptenKeyboardEvent* p_event, void*)
{
	if(p_event->altKey && p_event->keyCode == 13)
	{
		Engine::inst().toggleFullScreen();
		return EM_TRUE;
	}
	return EM_FALSE;
}

static void emscriptenSetFullScreen(bool fullScreen)
{
	// Not emscripten_request_fullscreen_strategy("#canvas"): that promotes the
	// canvas itself, and the browser then paints only it. The on-screen pad
	// sits beside it and would be invisible. b5_setFullscreen takes the root
	// element that holds both instead; the canvas fills the page anyway.
	EM_ASM({ Module['b5_setFullscreen']($0); }, fullScreen ? 1 : 0);
}

static EM_BOOL engineTouchFullScreen(int, const EmscriptenTouchEvent*, void*)
{
	// The audio from here as well, and not only from GS_Loading. Going
	// fullscreen turns the screen on a phone, and that makes the browser
	// cancel the touch in flight - SDL never sees the press, GS_Loading knows
	// nothing of the gesture and waits for a second one. Here the gesture is
	// real and unambiguous.
	WebAudio::resume();
	Engine::inst().enforceTouchFullScreen();
	// EM_FALSE: the touch still belongs to SDL. It is the click that takes the
	// loading screen further and the click the game is played with.
	return EM_FALSE;
}

bool Engine::isPhone() const
{
	return EM_ASM_INT({ return (Module['b5_isPhone'] && Module['b5_isPhone']()) ? 1 : 0; }) != 0;
}

void Engine::enforceTouchFullScreen()
{
	// Only on a device without a mouse. A notebook with a touchscreen has a
	// title bar somebody wants; a phone has none, and in mobile Chrome there
	// is no way at all to ask for the fullscreen by hand - the game therefore
	// takes it itself.
	if(!isPhone()) return;

	// The same condition as for Alt+Return: without a framebuffer object there
	// is no presentFrame() that would fill another area with black bars.
	if(!useFrameBuffer) return;

	// The browser is asked and not our own flag: leaving the fullscreen with a
	// swipe leaves fullScreen standing at true, and setFullScreen(true) would
	// then never reach the API at all.
	if(EM_ASM_INT({ return (document.fullscreenElement ||
							document.webkitFullscreenElement) ? 1 : 0; })) return;

	emscriptenSetFullScreen(true);
	fullScreen = true;
}
#endif

Vec2i Engine::getDesktopSize() const
{
#ifdef __EMSCRIPTEN__
	// In the browser the "desktop" is the window the page sits in.
	return Vec2i(EM_ASM_INT({ return window.innerWidth | 0; }),
				 EM_ASM_INT({ return window.innerHeight | 0; }));
#else
	// SDL_GetVideoInfo gives the desktop resolution only before the first
	// SDL_SetVideoMode, the window's afterwards - hence directly under Win32.
#ifdef _WIN32
	const int w = GetSystemMetrics(SM_CXSCREEN);
	const int h = GetSystemMetrics(SM_CYSCREEN);
	if(w > 0 && h > 0) return Vec2i(w, h);
#endif
	const SDL_VideoInfo* p_info = SDL_GetVideoInfo();
	if(p_info && p_info->current_w > 0 && p_info->current_h > 0)
		return Vec2i(p_info->current_w, p_info->current_h);
	return screenSize;
#endif
}

Vec2i Engine::getDefaultWindowSize() const
{
	// Integer multiples, to let Sharp get by without black bars too. The
	// margin is measured for 1920x1080 to still get twice the size: 2*480 is
	// 960, and 1080-120 is 960 as well. The same value horizontally.
	const int margin = 120;
	const Vec2i desktop = getDesktopSize();
	int scale = 1;
	while((scale + 1) * screenSize.x <= desktop.x - margin &&
		  (scale + 1) * screenSize.y <= desktop.y - margin) scale++;
	return screenSize * scale;
}

void Engine::rememberWindowPlacement()
{
#ifdef _WIN32
	// In fullscreen the window sits at (0,0) and is screen-sized. What counts
	// then is what applyWindowStyle() remembered before the switch.
	if(fullScreen)
	{
		if(savedWindowStyle)
		{
			windowedPosition = Vec2i(savedWindowRect[0], savedWindowRect[1]);
			windowedPositionKnown = true;
		}
		return;
	}

	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if(!SDL_GetWMInfo(&info) || !info.window) return;

	// GetWindowPlacement rather than GetWindowRect: for a maximized window
	// GetWindowRect gives the maximized frame. rcNormalPosition is what
	// "restore" goes back to, and that is what gets saved.
	WINDOWPLACEMENT wp;
	wp.length = sizeof(wp);
	if(!GetWindowPlacement(info.window, &wp)) return;

	maximized = (wp.showCmd == SW_SHOWMAXIMIZED);
	windowedPosition = Vec2i(wp.rcNormalPosition.left, wp.rcNormalPosition.top);
	windowedPositionKnown = true;

	// rcNormalPosition is a window rect, windowedSize a client area.
	// AdjustWindowRectEx on an empty rectangle gives exactly the frame.
	RECT frame = { 0, 0, 0, 0 };
	const LONG style   = GetWindowLong(info.window, GWL_STYLE);
	const LONG exStyle = GetWindowLong(info.window, GWL_EXSTYLE);
	if(AdjustWindowRectEx(&frame, style & ~WS_MAXIMIZE, FALSE, exStyle))
	{
		const int w = (wp.rcNormalPosition.right  - wp.rcNormalPosition.left) - (frame.right  - frame.left);
		const int h = (wp.rcNormalPosition.bottom - wp.rcNormalPosition.top)  - (frame.bottom - frame.top);
		if(w >= screenSize.x && h >= screenSize.y) windowedSize = Vec2i(w, h);
	}
#endif
}

void Engine::restoreWindowPosition()
{
#ifdef _WIN32
	if(!windowedPositionKnown) return;

	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if(!SDL_GetWMInfo(&info) || !info.window) return;

	// If the window would land on no screen at all, better leave it where
	// Windows put it. MonitorFromRect answers that correctly for negative
	// coordinates too, which a monitor to the left of the first one has.
	RECT r;
	r.left   = windowedPosition.x;
	r.top    = windowedPosition.y;
	r.right  = windowedPosition.x + displaySize.x;
	r.bottom = windowedPosition.y + displaySize.y;
	if(!MonitorFromRect(&r, MONITOR_DEFAULTTONULL)) return;

	SetWindowPos(info.window, HWND_NOTOPMOST, windowedPosition.x, windowedPosition.y,
				 0, 0, SWP_NOSIZE | SWP_NOZORDER);

	// Maximized before, maximized again. SDL turns that into an
	// SDL_VIDEORESIZE of its own accord, which handleResize() picks up.
	if(maximized) ShowWindow(info.window, SW_MAXIMIZE);
#endif
}

bool Engine::isWindowMaximized() const
{
#ifdef _WIN32
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if(SDL_GetWMInfo(&info) && info.window) return IsZoomed(info.window) != 0;
#endif
	return false;
}

#ifdef _WIN32
// Windows stops the application for as long as the user holds the window
// border or the title bar: DefWindowProc runs a message loop of its own and
// the main loop sits stuck in SDL_PollEvent. The only thing still running
// is the window procedure, and one of ours therefore goes in front of SDL's.
static WNDPROC p_sdlWindowProc = 0;
static const UINT_PTR SIZEMOVE_TIMER_ID = 0xB5;

static LRESULT CALLBACK engineWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// Should not happen, but a null pointer in CallWindowProc would be a crash
	// on quit.
	if(!p_sdlWindowProc) return DefWindowProc(hwnd, msg, wParam, lParam);

	Engine& engine = Engine::inst();

	switch(msg)
	{
	case WM_ENTERSIZEMOVE:
		engine.setInSizeMove(true);
		// For the case where the user holds the border without moving it: no
		// more WM_SIZE arrives then. During a movement WM_TIMER is crowded
		// out - but that is exactly when WM_SIZE draws.
		SetTimer(hwnd, SIZEMOVE_TIMER_ID, 15, 0);
		break;

	case WM_EXITSIZEMOVE:
		KillTimer(hwnd, SIZEMOVE_TIMER_ID);
		engine.setInSizeMove(false);
		// Nothing is tidied up here: handleResize() pulls the SDL side along as
		// soon as the main loop runs again.
		break;

	case WM_TIMER:
		if(wParam == SIZEMOVE_TIMER_ID)
		{
			engine.repaintDuringSizeMove();
			return 0;
		}
		break;

	case WM_SIZE:
		// The real source during the drag: it comes at every step.
		if(wParam != SIZE_MINIMIZED) engine.repaintDuringSizeMove();
		break;

	case WM_PAINT:
		// During a file dialog WM_PAINT arrives as soon as the dialog releases
		// the window. ValidateRect is mandatory: a WM_PAINT that is not cleared
		// comes straight back and goes round in circles.
		if(engine.isInSizeMove())
		{
			engine.repaintDuringSizeMove();
			ValidateRect(hwnd, 0);
			return 0;
		}
		break;

	case WM_GETMINMAXINFO:
		{
			// handleResize() clamps up to 640x480 anyway; this tells Windows
			// as much during the drag already. Chain first, then change:
			// DefWindowProc fills in four other members of the structure.
			const LRESULT result = CallWindowProc(p_sdlWindowProc, hwnd, msg, wParam, lParam);

			const Vec2i minimum = engine.getMinimumWindowSize();
			if(minimum.x > 0 && minimum.y > 0)
			{
				MINMAXINFO* p_info = reinterpret_cast<MINMAXINFO*>(lParam);
				p_info->ptMinTrackSize.x = minimum.x;
				p_info->ptMinTrackSize.y = minimum.y;

				// Without a framebuffer object the lower bound is the upper
				// one as well. The style alone ought to be enough, but
				// Windows has ways to get at the window that do not drag
				// a border - Win+Arrow for one.
				if(engine.hasFixedWindowSize())
				{
					p_info->ptMaxTrackSize.x = minimum.x;
					p_info->ptMaxTrackSize.y = minimum.y;
					p_info->ptMaxSize.x      = minimum.x;
					p_info->ptMaxSize.y      = minimum.y;
				}
			}

			return result;
		}
	}

	return CallWindowProc(p_sdlWindowProc, hwnd, msg, wParam, lParam);
}

void Engine::hookWindowProc()
{
	if(p_sdlWindowProc) return;

	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if(!SDL_GetWMInfo(&info) || !info.window) return;

	p_sdlWindowProc = reinterpret_cast<WNDPROC>(
		SetWindowLongPtr(info.window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(engineWindowProc)));
}

void Engine::unhookWindowProc()
{
	if(!p_sdlWindowProc) return;

	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if(SDL_GetWMInfo(&info) && info.window)
	{
		KillTimer(info.window, SIZEMOVE_TIMER_ID);
		SetWindowLongPtr(info.window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(p_sdlWindowProc));
	}

	p_sdlWindowProc = 0;
}

Vec2i Engine::getMinimumWindowSize() const
{
	if(fullScreen) return Vec2i(0, 0);   // in fullscreen nobody drags a border

	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if(!SDL_GetWMInfo(&info) || !info.window) return Vec2i(0, 0);

	// From the wanted client area to the window rect: the frame and the title
	// bar come on top, and only Windows knows how thick those are.
	RECT r = { 0, 0, screenSize.x, screenSize.y };
	const LONG style   = GetWindowLong(info.window, GWL_STYLE);
	const LONG exStyle = GetWindowLong(info.window, GWL_EXSTYLE);
	if(!AdjustWindowRectEx(&r, style, FALSE, exStyle)) return Vec2i(0, 0);

	return Vec2i(r.right - r.left, r.bottom - r.top);
}



void Engine::beginForeignMessageLoop()
{
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if(!SDL_GetWMInfo(&info) || !info.window) return;

	// The same state as dragging the window border: a foreign loop pumps the
	// messages. The timer keeps the picture fresh then too.
	inSizeMove = true;
	SetTimer(info.window, SIZEMOVE_TIMER_ID, 15, 0);

	// Once immediately: the dialog takes a moment to come up, and until then
	// the window would otherwise show whatever happens to lie in it.
	repaintDuringSizeMove();
}

void Engine::endForeignMessageLoop()
{
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if(SDL_GetWMInfo(&info) && info.window) KillTimer(info.window, SIZEMOVE_TIMER_ID);
	inSizeMove = false;
}

void Engine::repaintDuringSizeMove()
{
	// Only during the foreign message loop. Outside it the main loop draws,
	// and nothing may cut in on it.
	if(!inSizeMove || !initialized || !useFrameBuffer) return;

	// SwapBuffers can itself deliver messages; a second pass in the middle of
	// the first would be bad.
	static bool busy = false;
	if(busy) return;
	busy = true;

	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	RECT client;
	if(SDL_GetWMInfo(&info) && info.window && GetClientRect(info.window, &client))
	{
		const int w = client.right - client.left;
		const int h = client.bottom - client.top;

		// displaySize is only borrowed, not set - SDL_SetVideoMode calls
		// SetWindowPos and would fight the user's mouse. handleResize()
		// recognises a change only if what stands here is what SDL knows.
		if(w > 0 && h > 0)
		{
			const Vec2i knownToSDL = displaySize;

			displaySize = Vec2i(w, h);

			// The framebuffer object holds the last rendered frame - exactly
			// that now goes on the screen at the new size, with black bars and
			// filter.
			showLastFrame();

			displaySize = knownToSDL;

			// The main loop bound the framebuffer object before it came to a
			// stop in SDL_PollEvent, and goes on reading it afterwards - for
			// the video recording and for the crossfade. Leave it exactly as
			// found; the viewport comes back along with it.
			bindFrameBuffer();
		}
	}

	busy = false;
}
#endif

void Engine::fixWindowSize()
{
	// Without a framebuffer object the game draws straight into the back
	// buffer. The viewport is 640x480 and there is no presentFrame(), and a
	// larger window therefore does not fill with a larger picture - it shows
	// the same picture somewhere else, and the mouse mapping, which believes
	// displaySize, misses. handleResize() clamps the size to 640x480 anyway;
	// this tells the window itself, keeping it from growing in the first place.
#ifdef _WIN32
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if(!SDL_GetWMInfo(&info) || !info.window) return;

	HWND hwnd = info.window;

	// Restore first: restoreWindowPosition() runs before the decision about
	// the framebuffer object, and a window remembered as maximized would still
	// stand that way here. A SetWindowPos alone does not take the flag off it.
	if(IsZoomed(hwnd)) ShowWindow(hwnd, SW_RESTORE);

	// WS_THICKFRAME is the grab handle at the border, WS_MAXIMIZEBOX the
	// button - and with it the double click on the title bar goes too. Change
	// the style first, then measure: getMinimumWindowSize() reckons with the
	// one that is set.
	SetWindowLong(hwnd, GWL_STYLE,
				  GetWindowLong(hwnd, GWL_STYLE) & ~(WS_THICKFRAME | WS_MAXIMIZEBOX));

	const Vec2i frame = getMinimumWindowSize();
	if(frame.x > 0 && frame.y > 0)
	{
		SetWindowPos(hwnd, 0, 0, 0, frame.x, frame.y,
					 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
	}
#elif !defined(__EMSCRIPTEN__)
	LinuxWindow::setFixedSize(screenSize.x, screenSize.y);
#endif
}

void Engine::applyWindowStyle(bool wantFullScreen, const Vec2i& size)
{
	// SDL's flags are deliberately left alone: SDL_FULLSCREEN or SDL_NOFRAME
	// force DIB_SetVideoMode onto the slow path, and that calls
	// WIN_GL_ShutDown - the GL context and every texture would be gone. The
	// same holds under X11, for the same reason: X11_SetVideoMode rebuilds the
	// window for a mode change.
#ifdef _WIN32
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if(SDL_GetWMInfo(&info) && info.window)
	{
		HWND hwnd = info.window;
		if(wantFullScreen)
		{
			// Only the first time: a second pass would remember the WS_POPUP
			// that is already set - there would be no way out of fullscreen.
			if(!savedWindowStyle)
			{
				savedWindowStyle = static_cast<long>(GetWindowLong(hwnd, GWL_STYLE));
				RECT r;
				if(GetWindowRect(hwnd, &r))
				{
					savedWindowRect[0] = r.left;
					savedWindowRect[1] = r.top;
					savedWindowRect[2] = r.right - r.left;
					savedWindowRect[3] = r.bottom - r.top;
				}
			}

			SetWindowLong(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
			// HWND_TOP, not HWND_TOPMOST: a borderless fullscreen window that
			// sticks above everything makes Alt+Tab useless.
			SetWindowPos(hwnd, HWND_TOP, 0, 0, size.x, size.y,
						 SWP_FRAMECHANGED | SWP_SHOWWINDOW);
		}
		else
		{
			long style = savedWindowStyle;
			int x = savedWindowRect[0], y = savedWindowRect[1];
			int w = savedWindowRect[2], h = savedWindowRect[3];

			if(!style)
			{
				// Nothing remembered - back into a window all the same. There
				// must always be a way out of fullscreen.
				style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
				RECT r = { 0, 0, size.x, size.y };
				AdjustWindowRect(&r, style, FALSE);
				w = r.right - r.left;
				h = r.bottom - r.top;
				const Vec2i desktop = getDesktopSize();
				x = (desktop.x - w) / 2;
				y = (desktop.y - h) / 2;
				if(x < 0) x = 0;
				if(y < 0) y = 0;
			}

			SetWindowLong(hwnd, GWL_STYLE, style);
			SetWindowPos(hwnd, HWND_NOTOPMOST, x, y, w, h,
						 SWP_FRAMECHANGED | SWP_SHOWWINDOW);
			savedWindowStyle = 0;
		}
	}
#elif !defined(__EMSCRIPTEN__)
	// Under X11 the window manager decides how large a fullscreen window
	// becomes and where it sits. Once it has accepted the request there is
	// nothing left to do here: the new size is not settled yet and arrives in
	// a moment as SDL_VIDEORESIZE. Forcing it now would mean letting
	// SDL_SetVideoMode work against the window manager.
	if(LinuxWindow::setFullScreen(wantFullScreen)) return;
#endif

	// Always through here: displaySize belongs to handleResize, and SDL has to
	// learn the new size - otherwise the mouse cursor is stuck on the old area.
	handleResize(size.x, size.y);
}

void Engine::setFullScreen(bool wantFullScreen)
{
	// Without a framebuffer object the picture stays at 640x480, see
	// handleResize().
	if(wantFullScreen && initialized && !useFrameBuffer) return;

	if(!initialized || fullScreen == wantFullScreen) { fullScreen = wantFullScreen; return; }

	fullScreen = wantFullScreen;
	printfLog("* %s\n", wantFullScreen ? "Going fullscreen" : "Leaving fullscreen");

#ifdef __EMSCRIPTEN__
	// In the browser the Fullscreen API does this, and it demands a real key
	// press - hence only from engineFullScreenHotkey() at the DOM.
	emscriptenSetFullScreen(wantFullScreen);
#else
	applyWindowStyle(wantFullScreen, wantFullScreen ? getDesktopSize() : windowedSize);
#endif
}

void Engine::handleResize(int width, int height)
{
	// Without a framebuffer object the game draws straight into the back
	// buffer: there is no presentFrame() to pick up a different window size,
	// the viewport has been 640x480 since init(), and the mouse mapping and the
	// crossfade reckon with that too. The window therefore keeps its size
	// instead of showing a picture in the corner.
	if(!useFrameBuffer)
	{
		width  = screenSize.x;
		height = screenSize.y;
	}

#ifndef __EMSCRIPTEN__
	// The window may not become smaller than the internal picture: below that
	// Sharp has no integer step left. In the browser the canvas sets the size,
	// and pushing against that ended in an infinite loop.
	if(width  < screenSize.x) width  = screenSize.x;
	if(height < screenSize.y) height = screenSize.y;
#endif

	if(width <= 0 || height <= 0) return;
	if(width == displaySize.x && height == displaySize.y) return;

#ifndef __EMSCRIPTEN__
	// The same flags as the first time, or the fast path is lost.
	SDL_Surface* p_new = SDL_SetVideoMode(width, height, 32, SDL_OPENGL | SDL_RESIZABLE);
	if(!p_new)
	{
		printfLog("- WARNING: Could not resize to %dx%d (%s).\n", width, height, SDL_GetError());
		return;
	}
	p_display = p_new;
#endif

	displaySize = Vec2i(width, height);
	// Do not write down a maximized size: the remembered window size would
	// then be the maximized window's, and "restore" would have no target left.
	if(useFrameBuffer && !fullScreen && !isWindowMaximized()) windowedSize = displaySize;
}

Vec2d Engine::warpToSource(const Vec2d& p) const
{
	return getEffectiveUpscaler()->warpToSource(p);
}

Vec2d Engine::warpToOutput(const Vec2d& p) const
{
	return getEffectiveUpscaler()->warpToOutput(p);
}

void Engine::computePresentRect(int& x, int& y, int& w, int& h) const
{
	// The largest possible 4:3 rectangle in the window, centred. What is left
	// over goes black - black bars rather than a distorted picture.
	double scale = min(static_cast<double>(displaySize.x) / screenSize.x,
					   static_cast<double>(displaySize.y) / screenSize.y);

	// Sharp needs an integer step. At a fractional factor nearest doubles some
	// source pixels and not others - uneven stroke widths, ragged lettering.
	// Below 1:1 there is no such step.
	if(getEffectiveUpscaler()->wantsIntegerScale() && scale >= 1.0) scale = floor(scale);

	w = static_cast<int>(screenSize.x * scale);
	h = static_cast<int>(screenSize.y * scale);
	x = (displaySize.x - w) / 2;
	y = (displaySize.y - h) / 2;
}

void Engine::presentFrame()
{
	if(!useFrameBuffer) return;

	int x, y, w, h;
	computePresentRect(x, y, w, h);

	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glDisable(GL_BLEND);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_SCISSOR_TEST);
	glEnable(GL_TEXTURE_2D);
	glColor4d(1.0, 1.0, 1.0, 1.0);

	glMatrixMode(GL_TEXTURE);
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	gluOrtho2D(0.0, displaySize.x, 0.0, displaySize.y);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glClearColor(0.0, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);

	glBindTexture(GL_TEXTURE_2D, frameTextureID);

	// From here on the frame belongs to the filter. What it needs is in the
	// context; what belongs to it - its shader, its sliders - only it knows.
	PresentContext context;
	context.rectPosition = Vec2i(x, y);
	context.rectSize     = Vec2i(w, h);
	context.displaySize  = displaySize;
	context.frameSize    = screenSize;
	context.textureSize  = frameTextureSize;
	context.textureID    = frameTextureID;
	context.vertexBuffer = presentVertexBuffer;

	Upscaler* p_upscaler = getEffectiveUpscaler();

	// Sharp and Smooth are nothing but this setting; SharpFit and the CRT
	// filter remap the texture coordinate for the hardware interpolation to
	// give the wanted result, and therefore need GL_LINEAR as well.
	const GLint filter = p_upscaler->getTextureFilter();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

	p_upscaler->present(context);

	glBindTexture(GL_TEXTURE_2D, 0);

	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_TEXTURE);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);

	glPopAttrib();
}

void Engine::drawOverlays()
{
	if(p_muteIconTexture && soundVolume == 0.0 && musicVolume == 0.0)
	{
		renderSprite(p_muteIconTexture, Vec2i(5, 5),
					 muteIconPositionOnTexture, muteIconSize, Vec4d(1.0, 1.0, 1.0, 0.75));
	}

	if(p_recordingIconTexture && p_videoRecorder &&
	   ((getExactTimeMS() - recordingStartTime) / 500) % 2)
	{
		renderSprite(p_recordingIconTexture, Vec2i(screenSize.x - recordingIconSize.x - 5, 5),
					 recordingIconPositionOnTexture, recordingIconSize, Vec4d(1.0, 1.0, 1.0, 0.75));
	}

	if(performanceShown) drawPerformance();
}

// What the last few hundred frames cost, in the bottom left corner. This is
// how the numbers are read on a phone: there is no console there and no test
// harness, and how long a frame took is precisely what cannot be measured from
// outside. In a desktop browser the same numbers come out of the test hook
// instead, and then without the cost of drawing them.
//
// drawOverlays() runs after the frame and before the present, so this lands in
// neither the RENDER nor the PRESENT the block reports - and it lands in a
// screenshot, which on a phone is how the figure gets off the device at all.
//
// The bottom and not the top, although both corners are taken: at the bottom
// it covers the status bar, whose numbers stand still and can be read by
// turning the overlay off, and at the top it would cover the toasts, which
// slide past once and are how the game reports a fault.
void Engine::drawPerformance()
{
	Font* p_font = GUI::inst().getFont();
	if(!p_font) return;

	// The frame rate off the interval and the rest off the work: the two
	// differ whenever something else sets the pace, which in the browser
	// requestAnimationFrame always does.
	const float interval = frameStats.getPercentile(FrameStats::FS_INTERVAL, 50);

	char line[3][96];
	snprintf(line[0], sizeof(line[0]), "%.0f fps   frame %.1f %.1f %.1f ms  (50/95/max)",
			 interval > 0.0f ? 1000.0f / interval : 0.0f,
			 frameStats.getPercentile(FrameStats::FS_TOTAL, 50),
			 frameStats.getPercentile(FrameStats::FS_TOTAL, 95),
			 frameStats.getPercentile(FrameStats::FS_TOTAL, 100));
	snprintf(line[1], sizeof(line[1]), "render %.1f  update %.1f  present %.1f  swap %.1f",
			 frameStats.getPercentile(FrameStats::FS_RENDER, 50),
			 frameStats.getPercentile(FrameStats::FS_UPDATE, 50),
			 frameStats.getPercentile(FrameStats::FS_PRESENT, 50),
			 frameStats.getPercentile(FrameStats::FS_SWAP, 50));
	// The budget is the logic rate - 20 ms, fifty frames a second - and the
	// two counts against it answer different questions. A frame whose
	// *interval* went over is one the player did not get; one whose *work*
	// went over is one this game is responsible for. They come apart exactly
	// where it matters: under swiftshader the browser ran at 38 ms a frame on
	// 2.7 ms of work, so counting the work alone would have reported nothing
	// wrong while the game ran at 26 fps.
	//
	// 500 ms is a third question. That is what Emscripten's OpenAL has
	// scheduled ahead (AL.QUEUE_LOOKAHEAD, raised in initOpenAL), so a frame
	// longer than that is a hole in the music - in the browser only, since
	// natively the decoder thread fills the queue whatever the main thread is
	// doing.
	const float budget = static_cast<float>(logicRate);
	snprintf(line[2], sizeof(line[2]), "of %u frames: %u over %.0f ms, %u of work, %u over 500 ms",
			 frameStats.getCount(),
			 frameStats.getCountOver(FrameStats::FS_INTERVAL, budget),
			 budget,
			 frameStats.getCountOver(FrameStats::FS_TOTAL, budget),
			 frameStats.getCountOver(FrameStats::FS_TOTAL, 500.0f));

	const int lineHeight = p_font->getLineHeight();
	const int height = 3 * lineHeight + 8;
	const int top = screenSize.y - height;

	setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
	glDisable(GL_TEXTURE_2D);
	glBegin(GL_QUADS);
	glColor4d(0.0, 0.0, 0.0, 0.7);
	glVertex2i(0, top);
	glVertex2i(screenSize.x, top);
	glVertex2i(screenSize.x, screenSize.y);
	glVertex2i(0, screenSize.y);
	glEnd();
	glEnable(GL_TEXTURE_2D);

	for(int i = 0; i < 3; i++)
	{
		p_font->renderText(line[i], Vec2i(6, top + 4 + i * lineHeight), Vec4d(1.0, 1.0, 1.0, 1.0));
	}
}

bool Engine::screenshot()
{
	// Always the internal 640x480 frame: the filter and the black bars are
	// display settings and do not belong in the file.
	const Vec2i shotSize(useFrameBuffer ? screenSize : displaySize);

	// GL_RGBA and not GL_RGB or GL_BGR: that is the only combination WebGL 1
	// allows too. Only the three colour channels of it reach the file - see
	// img_save.h, the alpha would be a quarter more for nothing but 255. The
	// encoder flips the rows along the way; no second buffer is needed.
	std::vector<uchar> pixels(static_cast<size_t>(shotSize.x) * shotSize.y * 4);
	glReadBuffer(useFrameBuffer ? GL_COLOR_ATTACHMENT0_EXT : GL_BACK);
	glReadPixels(0, 0, shotSize.x, shotSize.y, GL_RGBA, GL_UNSIGNED_BYTE, &pixels[0]);

	std::vector<uchar> png;
	if(!encodePNG(&pixels[0], shotSize, 4, 3, true, &png))
	{
		printfLog("+ ERROR: Could not encode the screenshot.\n");
		return false;
	}

	char screenshotDateTime[256];
	const time_t t = ::time(0);
	strftime(screenshotDateTime, 256, "%Y-%m-%d@%H-%M-%S", localtime(&t));

#ifdef __EMSCRIPTEN__
	// In the browser there is no directory the picture would belong in: the
	// IndexedDB is there for saved games, and putting a picture in it would
	// mean filling the player's quota with something they never get to see
	// again. It goes straight into their downloads instead.
	char downloadName[512] = "";
	sprintf(downloadName, "blocks5_%s.png", screenshotDateTime);
	WebTransfer::downloadBytes(&png[0], static_cast<uint>(png.size()), downloadName);
	return true;
#else
	FileSystem& fs = FileSystem::inst();
	std::string filename;
	for(uint no = 1; true; no++)
	{
		char temp[512] = "";
		if(no == 1) sprintf(temp, "%s.png", screenshotDateTime);
		else sprintf(temp, "%s_%02d.png", screenshotDateTime, no);
		filename = fs.getAppHomeDirectory() + "screenshots/" + temp;
		if(!fs.fileExists(filename)) break;
	}

	File* p_file = fs.openFile(filename, FileSystem::FM_WRITE);
	if(!p_file)
	{
		printfLog("+ ERROR: Could not write \"%s\".\n", filename.c_str());
		return false;
	}

	const uint numBytes = static_cast<uint>(png.size());
	const bool saved = p_file->write(&png[0], numBytes) == numBytes && p_file->finish();
	fs.closeFile(p_file);
	if(!saved) printfLog("+ ERROR: Could not write \"%s\".\n", filename.c_str());
	return saved;
#endif
}

void Engine::renderSprite(const Vec2i& position,
						  const Vec2i& positionOnTexture,
						  const Vec2i& size,
						  const Vec4d& color,
						  bool mirrorX,
						  double rotation,
						  double scaling)
{
	if(renderSuppressed) return;

	// The quad runs from -halfSize to otherHalf, and the two are only the same
	// number while the size is even. Taking halfSize for both would draw an odd
	// sprite one pixel short while its texture coordinates still spanned all
	// size texels, so the picture is resampled and a row of it falls out: at
	// 39x39 texel 19 is never reached. Only the centre stays truncated, which
	// keeps the corners on whole pixels and a nearest-sampled sprite sharp - at
	// an odd size that puts the axis of the rotation half a pixel off the
	// middle, which is the cheaper of the two errors.
	const Vec2i halfSize(size / 2);
	const Vec2i otherHalf(size - halfSize);

	// Mirroring swaps the texture coordinates instead of scaling x by -1, and
	// that is not the same thing once the quad is no longer symmetric about its
	// centre: the scale reflects the footprint as well, which moves an odd
	// sprite a pixel to the left of where the unmirrored one stands.
	const int u0 = positionOnTexture.x + (mirrorX ? size.x : 0);
	const int u1 = positionOnTexture.x + (mirrorX ? 0 : size.x);
	const int v0 = positionOnTexture.y;
	const int v1 = positionOnTexture.y + size.y;

	// Before the bracket below, not inside it: the batch bakes the sprite's own
	// transform itself, and the matrix it reads back must be the caller's.
	if(spriteBatchOpen)
	{
		queueSprite(position, halfSize, otherHalf, u0, u1, v0, v1, color, rotation, scaling);
		return;
	}

	glPushMatrix();
	glTranslated(position.x + halfSize.x, position.y + halfSize.y, 0.0);
	if(scaling != 1.0) glScaled(scaling, scaling, 1.0);
	if(rotation != 0.0) glRotated(rotation, 0.0, 0.0, 1.0);

	glBegin(GL_QUADS);
	glColor4dv(color);
	glTexCoord2i(u0, v0);
	glVertex2i(-halfSize.x, -halfSize.y);
	glTexCoord2i(u1, v0);
	glVertex2i(otherHalf.x, -halfSize.y);
	glTexCoord2i(u1, v1);
	glVertex2i(otherHalf.x, otherHalf.y);
	glTexCoord2i(u0, v1);
	glVertex2i(-halfSize.x, otherHalf.y);
	glEnd();

	glPopMatrix();
}

// GL_QUADS out of a client array has two ceilings in the browser and at this
// stride they are the same number. The emulation's quad index table is a
// Uint16Array, so it wraps at vertex 65536; and it asserts that the vertices
// times the stride fit its 2 MiB scratch buffer, which at 32 bytes is again
// 65536. A level frame issues a few hundred quads, so this is a backstop.
const uint BATCH_MAX_QUADS = 16384;

void Engine::beginSpriteBatch()
{
	// Anything still queued belongs to whatever was drawing before this, and
	// drawing it now would be under the new pass's state. Empty in practice -
	// endSpriteBatch() sees to that - but a pass that ever returns early would
	// otherwise carry its quads into the next one.
	flushSprites();
	spriteBatchOpen = !spriteBatchDisabled;
}

void Engine::flushSprites()
{
	if(spriteBatch.empty()) return;

#ifdef BLOCKS5_TEST_HOOKS
	{
		GLint texture = 0;
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
		if(texture != batchTexture || glIsEnabled(GL_TEXTURE_2D) != batchTexturing)
		{
			printfLog("+ ERROR: sprite batch of %u quads was queued against texture %d/%d "
					  "and is being drawn against %d/%d - a flush is missing.\n",
					  static_cast<uint>(spriteBatch.size() / 4),
					  static_cast<int>(batchTexture), static_cast<int>(batchTexturing),
					  static_cast<int>(texture), static_cast<int>(glIsEnabled(GL_TEXTURE_2D)));
		}
	}
#endif

	// The vertices already carry the modelview they were queued under, so the
	// draw has to happen under none: otherwise GL applies it a second time.
	// Identity and not "the matrix the queue began with", because one batch
	// spans objects that each pushed their own. The projection still applies,
	// which is what puts the whole thing on the screen.
	//
	// The matrix mode is said rather than assumed, and the attrib bracket is
	// what says it. A flush happens wherever the state moves, which includes
	// Texture::bind() - and Level::render binds the snow and the clouds with
	// GL_TEXTURE current, so an unqualified glPushMatrix here would push, wipe
	// and pop the *texture* matrix and leave the sprites under whatever
	// modelview happened to stand. It is empty at that call today, which is the
	// only reason this has never shown; saying the mode costs two calls a flush
	// and stops it being a question.
	glPushAttrib(GL_TRANSFORM_BIT);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	drawQuadArray(&spriteBatch[0], static_cast<uint>(spriteBatch.size()));
	glPopMatrix();
	glPopAttrib();

	// The current colour is deliberately left alone. Immediate mode used to
	// leave the last sprite's colour standing, and putting that back here
	// looked like the faithful thing to do - but a flush happens wherever the
	// state moves, which includes the middle of somebody else's drawing.
	// Font::renderText sets its shadow colour and then calls drawText, whose
	// first act is a bind: restoring the colour there painted every shadow of
	// every string in the last sprite's colour instead.
	spriteBatch.clear();
}

void Engine::endSpriteBatch()
{
	flushSprites();
	spriteBatchOpen = false;
}

void Engine::queueSprite(const Vec2i& position,
						 const Vec2i& halfSize,
						 const Vec2i& otherHalf,
						 int u0,
						 int u1,
						 int v0,
						 int v1,
						 const Vec4d& color,
						 double rotation,
						 double scaling)
{
	if(spriteBatch.size() >= 4 * BATCH_MAX_QUADS) flushSprites();

#ifdef BLOCKS5_TEST_HOOKS
	// A queued quad is drawn with the state standing at the flush, not at the
	// call - so anything that moves that state in between has to flush first.
	// The static check reads the sources for it; this reads what actually
	// happened, and says which pass let it through.
	if(spriteBatch.empty())
	{
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &batchTexture);
		batchTexturing = glIsEnabled(GL_TEXTURE_2D);
	}
#endif

	// The transform the caller set up is read back rather than tracked. It is
	// whatever Object::render and everything above it pushed - the translate
	// to the object's cell, the squash of a teleporting object, the unbalanced
	// glTranslated Enemy does inside its own onRender, the half pixel
	// Level::render puts under the wires - and baking it is what lets sprites
	// from different objects share one draw call. Reading it costs one call
	// against the seven this path no longer makes: in the browser a copy of
	// sixteen floats out of a JavaScript array, on a desktop client-side
	// driver state and not a pipeline stall.
	GLfloat m[16];
	glGetFloatv(GL_MODELVIEW_MATRIX, m);

	// The sprite's own transform, in the order immediate mode applies it.
	// Mirroring is already in the texture coordinates, so what is left is
	// rotate, then scale, then translate to the centre.
	double c = scaling;
	double s = 0.0;
	if(rotation != 0.0)
	{
		const double a = rotation * (3.1415926535897932384626433832795 / 180.0);
		c = scaling * cos(a);
		s = scaling * sin(a);
	}

	const double tx = position.x + halfSize.x;
	const double ty = position.y + halfSize.y;

	// Widened one at a time and not inside the braces: a braced initializer
	// list forbids a narrowing conversion, and clang says so where gcc does not.
	const double left = -halfSize.x, right = otherHalf.x;
	const double top = -halfSize.y, bottom = otherHalf.y;
	const double lx[4] = {left, right, right, left};
	const double ly[4] = {top, top, bottom, bottom};
	const int u[4] = {u0, u1, u1, u0};
	const int v[4] = {v0, v0, v1, v1};

	// clampColor, because renderShine hands this deathCountDown * 5.0 from an
	// exploding bomb and expects GL to cut it off. Immediate mode cut it off
	// twice over - the hardware by specification, and Emscripten by quantising
	// the value to a byte - and a colour array has neither.
	const Vec4d cut = clampColor(color);
	const Vec4f col(static_cast<float>(cut.r), static_cast<float>(cut.g),
					static_cast<float>(cut.b), static_cast<float>(cut.a));

	for(int i = 0; i < 4; i++)
	{
		const double x = tx + c * lx[i] - s * ly[i];
		const double y = ty + s * lx[i] + c * ly[i];

		// The modelview is column major and, in this game, always an affine
		// map of the plane: z is never anything but 0, so two columns and the
		// translation are the whole of it.
		ColorQuadVertex vertex;
		vertex.position = Vec2f(static_cast<float>(m[0] * x + m[4] * y + m[12]),
								static_cast<float>(m[1] * x + m[5] * y + m[13]));
		vertex.uv = Vec2f(static_cast<float>(u[i]), static_cast<float>(v[i]));
		vertex.color = col;
		spriteBatch.push_back(vertex);
	}
}

void Engine::renderSprite(Texture* p_sprite,
						  const Vec2i& position,
						  const Vec2i& positionOnTexture,
						  const Vec2i& size,
						  const Vec4d& color,
						  bool mirrorX,
						  double rotation,
						  double scaling)
{
	p_sprite->bind();
	renderSprite(position, positionOnTexture, size, color, mirrorX, rotation, scaling);
	p_sprite->unbind();
}

void Engine::renderSprites(const Sprites& sprites,
						   const Vec4d& color)
{
	const int numSprites = sprites.getCount();
	for(int i = 0; i < numSprites; i++)
	{
		const Sprite& sprite = sprites[i];
		renderSprite(sprite.offset,
					 sprite.positionOnTexture,
					 sprite.size,
					 color * sprite.color,
					 sprite.mirrorX,
					 sprite.rotation);
	}
}

SoundInstance* Engine::playSound(const std::string& filename,
								 bool loop,
								 double pitchSpectrum,
								 int priority,
								 bool forceCreation)
{
	if(!filename.length()) return 0;

	Sound* p_sound = Manager<Sound>::inst().request(filename);
	if(p_sound)
	{
		SoundInstance* p_inst = p_sound->createInstance(forceCreation);
		p_sound->release();

		if(p_inst)
		{
			// set the pitch
			if(pitchSpectrum != 0.0) p_inst->setPitch(1.0 + random(-pitchSpectrum, pitchSpectrum));

			// set the priority
			p_inst->setPriority(priority);

			// play
			p_inst->play(loop);
		}

		return p_inst;
	}

	return 0;
}

void Engine::setBlendFunc(GLenum srcRGB,
						  GLenum dstRGB,
						  GLenum srcAlpha,
						  GLenum dstAlpha)
{
	// Queued sprites were queued to be blended the old way.
	flushSprites();

	if(glExtBlendFuncSeparate) glExtBlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
	else glBlendFunc(srcRGB, dstRGB);
}

void Engine::registerGameState(GameState* p_gs)
{
	gameStates[p_gs->getName()] = p_gs;
}

GameState* Engine::findGameState(const std::string& gs)
{
	std::unordered_map<std::string, GameState*>::const_iterator i = gameStates.find(gs);
	if(i == gameStates.end()) return 0;
	else return i->second;
}

void Engine::setGameState(const std::string& gs,
						  const ParameterBlock& context)
{
	this->context = context;
	GameState* p_newGS = findGameState(gs);

	// the current state loses the focus
	p_stateToLoseFocus = getGameState();

	// leave all states
	while(!currentGameStates.empty())
	{
		GameState* p_gs = currentGameStates.top();
		statesToBeLeft.push_back(p_gs);
		currentGameStates.pop();
	}

	if(p_newGS)
	{
		// enter the new state
		currentGameStates.push(p_newGS);
		p_stateToBeEntered = p_newGS;
		p_stateToGetFocus = p_newGS;
	}
}

void Engine::pushGameState(const std::string& gs,
						   const ParameterBlock& context)
{
	this->context = context;
	GameState* p_newGS = findGameState(gs);

	// the current state loses the focus
	GameState* p_currentGS = getGameState();
	if(p_currentGS) p_stateToLoseFocus = p_currentGS;

	// enter the new state
	currentGameStates.push(p_newGS);
	p_stateToBeEntered = p_newGS;
	p_stateToGetFocus = p_newGS;
}

GameState* Engine::popGameState(const ParameterBlock& context)
{
	GameState* p_currentGS = getGameState();
	if(p_currentGS)
	{
		// leave the current state
		currentGameStates.pop();
		GameState* p_newGS = getGameState();
		p_stateToLoseFocus = p_currentGS;
		statesToBeLeft.push_back(p_currentGS);

		// the new state gets the focus
		if(p_newGS) p_stateToGetFocus = p_newGS;
	}

	return p_currentGS;
}

GameState* Engine::getGameState()
{
	if(currentGameStates.empty()) return 0;
	else return currentGameStates.top();
}

void Engine::processGameStateChanges()
{
	const bool changing = p_stateToLoseFocus || p_stateToBeEntered ||
						  p_stateToGetFocus || !statesToBeLeft.empty();

	// carry out the state change
	if(p_stateToLoseFocus) p_stateToLoseFocus->onLoseFocus();
	while(!statesToBeLeft.empty())
	{
		statesToBeLeft.front()->onLeave(context);
		statesToBeLeft.erase(statesToBeLeft.begin());
	}

	if(p_stateToBeEntered) p_stateToBeEntered->onEnter(context);
	if(p_stateToGetFocus) p_stateToGetFocus->onGetFocus();

	p_stateToBeEntered = p_stateToGetFocus = p_stateToLoseFocus = 0;

	// An edge belongs to the state that ran at the moment it got measured.
	// The new one does not inherit it, because in this tick updateActions() ran
	// long before it and GUI::update() only triggered the change afterwards.
	//
	// Otherwise one key would do two things at once: F5 means "play" in the
	// editor and "restart the level" in the game, and one press would trigger
	// both - the editor pushes GS_Game, and its first onUpdate() in the same
	// tick would see the edge still standing and restart the level it had just
	// loaded at once, rewind crossfade and all in place of the mosaic. The same
	// holds for Return, which starts in the level selection and saves at the
	// hotel in the game.
	if(changing) clearActionEdges();
}

void Engine::playMusic(const std::string& filename,
					   double loopBegin,
					   bool resumeWhereStopped)
{
	// Does the music have to change?
	if(currentMusicFilename != filename)
	{
		stopMusic();

		currentMusicFilename = filename;

		if(!filename.empty())
		{
			// load the new music
			p_currentMusic = Manager<StreamedSound>::inst().request(filename);
			if(p_currentMusic)
			{
				if(resumeWhereStopped)
				{
					// resume where music was last stopped
					std::unordered_map<std::string, uint>::const_iterator it = musicStoppedAt.find(filename);
					if(it != musicStoppedAt.end()) p_currentMusic->seekStream(it->second);
				}

				p_currentMusic->setVolume(0.0);
				p_currentMusic->play(loopBegin != -1.0);
				p_currentMusic->slideVolume(1.0, 0.02);
				p_currentMusic->setLoopBegin(loopBegin);
			}
			else
			{
				// Names the bare filename: for a campaign the full path leads
				// through the archive and its password.
				const std::string::size_type slash = filename.find_last_of('/');
				showToast(TOAST_ERROR, localizeString("$ERROR_MUSIC_MISSING") + " \"" +
									   (slash == std::string::npos ? filename : filename.substr(slash + 1)) + "\"");
			}
		}
	}
}

void Engine::stopMusic()
{
	if(p_currentMusic)
	{
		// remember where the music was stopped (more or less, this just asks
		// the audio stream's read cursor)
		musicStoppedAt[currentMusicFilename] = p_currentMusic->tellStream();

		p_currentMusic->slideVolume(-1.0, 0.02);
		p_currentMusic = 0;
	}

	currentMusicFilename = "";
}

bool Engine::isKeyDown(SDLKey key) const
{
	if(key < 0 || key >= NUM_KEY_SLOTS) return false;
	return keyData[key] & 1 ? true : false;
}

bool Engine::wasKeyPressed(SDLKey key) const
{
	if(key < 0 || key >= NUM_KEY_SLOTS) return false;
	return keyData[key] & 2 ? true : false;
}

void Engine::consumeKeyPress(SDLKey key)
{
	if(key < 0 || key >= NUM_KEY_SLOTS) return;
	keyData[key] &= ~2;
}

bool Engine::wasKeyReleased(SDLKey key) const
{
	if(key < 0 || key >= NUM_KEY_SLOTS) return false;
	return keyData[key] & 4 ? true : false;
}

bool Engine::wasAnyKeyPressed() const
{
	for(int i = 0; i < NUM_KEY_SLOTS; i++) if(keyData[i] & 2) return true;
	return false;
}

bool Engine::wasAnyButtonPressed() const
{
	for(int i = 0; i < NUM_KEY_SLOTS; i++) if(buttonData[i] & 2) return true;
	return false;
}

void Engine::setKeyDown(SDLKey key,
						bool status)
{
	if(key < 0 || key >= NUM_KEY_SLOTS) return;
	if(status) keyData[key] |= 1;
	else keyData[key] &= ~1;
}

void Engine::setKeyPressed(SDLKey key,
						   bool status)
{
	if(key < 0 || key >= NUM_KEY_SLOTS) return;
	if(status) keyData[key] |= 2;
	else keyData[key] &= ~2;
}

void Engine::setKeyReleased(SDLKey key,
							bool status)
{
	if(key < 0 || key >= NUM_KEY_SLOTS) return;
	if(status) keyData[key] |= 4;
	else keyData[key] &= ~4;
}

void Engine::setKeyData(SDLKey key,
						int data)
{
	if(key < 0 || key >= NUM_KEY_SLOTS) return;
	keyData[key] = data;
}

bool Engine::isButtonDown(uint button) const
{
	if(button >= NUM_KEY_SLOTS) return false;
	return buttonData[button] & 1 ? true : false;
}

bool Engine::wasButtonPressed(uint button) const
{
	if(button >= NUM_KEY_SLOTS) return false;
	return buttonData[button] & 2 ? true : false;
}

bool Engine::wasButtonReleased(uint button) const
{
	if(button >= NUM_KEY_SLOTS) return false;
	return buttonData[button] & 4 ? true : false;
}

bool Engine::getKeyEvent(SDL_KeyboardEvent* p_out, bool* p_repeat)
{
	if(keyEventQueue.empty()) return false;
	else
	{
		*p_out = keyEventQueue.front().event;
		if(p_repeat) *p_repeat = keyEventQueue.front().repeat;
		keyEventQueue.pop();
		return true;
	}
}

bool Engine::isGUIFocused()
{
	return GUI::inst().getFocusElement() != GUI::inst().getRoot();
}

void Engine::unfocusGUI()
{
	GUI::inst().setFocusElement(0);
}

const std::vector<VirtualKey>& Engine::getVKs() const
{
	return virtualKeys;
}

const std::unordered_map<std::string, Action*>& Engine::getActions() const
{
	return actions;
}

const std::vector<Action*>& Engine::getActionsVector() const
{
	return actionsVector;
}

int Engine::getKeyboardVK(SDLKey key) const
{
	return key;
}

const std::string& Engine::getVKId(int vk) const
{
	static const std::string empty;
	if(vk < 0 || vk >= static_cast<int>(virtualKeys.size())) return empty;
	return virtualKeys[vk].id;
}

int Engine::getVKFromId(const std::string& id) const
{
	if(id.empty()) return -1;

	for(uint i = 0; i < virtualKeys.size(); i++)
	{
		if(virtualKeys[i].id == id) return static_cast<int>(i);
	}

	// Unknown - a joystick that is not connected right now, say. Leaving it
	// unbound is better than a number picked on the off chance.
	return -1;
}

Action* Engine::registerAction(const std::string& name,
							   int primary,
							   int secondary)
{
	Action* p_action = new Action;
	p_action->name = name;
	p_action->primary = primary;
	p_action->secondary = secondary;
	p_action->repeats = true;
	p_action->delay = 240;
	p_action->interval = 80;
	p_action->data = 0;
	p_action->countDown = 0;
	p_action->buffered = 0;
	p_action->defaultPrimary = primary;
	p_action->defaultSecondary = secondary;

	actionsVector.push_back(p_action);
	actions[name] = p_action;

	return p_action;
}

void Engine::changeAction(const std::string& name,
						  int primary,
						  int secondary)
{
	Action* p_action = getAction(name);
	if(!getAction(name)) return;

	p_action->primary = primary;
	p_action->secondary = secondary;

	// The new key is as a rule still down - the player has only just pressed it
	// to bind it. Without the state being brought up to date here, the next
	// updateActions() would see a fresh edge and the action would fire once
	// immediately.
	syncActionDown(*p_action);
}

Action* Engine::getAction(const std::string& name) const
{
	std::unordered_map<std::string, Action*>::const_iterator it = actions.find(name);
	return it == actions.end() ? 0 : it->second;
}

bool Engine::isActionDown(const std::string& name) const
{
	const Action* p_action = getAction(name);
	if(!p_action) return false;
	if(p_action->data & 8) return false;
	return (p_action->data & 1) ? true : false;
}

bool Engine::wasActionPressed(const std::string& name) const
{
	const Action* p_action = getAction(name);
	if(!p_action) return false;
	if(p_action->data & 8) return false;
	return (p_action->data & 2) ? true : false;
}

bool Engine::wasActionReleased(const std::string& name) const
{
	const Action* p_action = getAction(name);
	return p_action ? ((p_action->data & 4) ? true : false) : false;
}

void Engine::updateVKs()
{
	// poll the keyboard and the joysticks
	SDL_PumpEvents();
#ifdef __EMSCRIPTEN__
	Uint8* p_keys = SDL_GetKeyboardState(0);
#else
	Uint8* p_keys = SDL_GetKeyState(0);
#endif
	SDL_JoystickUpdate();

	for(std::vector<VirtualKey>::iterator it = virtualKeys.begin();
		it != virtualKeys.end();
		++it)
	{
		VirtualKey& vk = *it;
		if(vk.device == -1)
		{
			// key
			vk.down = p_keys[vk.key] ? true : false;
		}
		else
		{
			// joystick
			SDL_Joystick* p_joystick = joysticks[vk.device];
			if(vk.key != -1)
			{
				// button
				vk.down = SDL_JoystickGetButton(p_joystick, vk.key) ? true : false;
			}
			else if(vk.axis != -1)
			{
				// axis
				int value = SDL_JoystickGetAxis(p_joystick, vk.axis);
				if(vk.positive)
				{
					if(vk.down) vk.down = value >= 7500;
					else vk.down = value >= 10000;
				}
				else
				{
					if(vk.down) vk.down = value <= -7500;
					else vk.down = value <= -10000;
				}
			}
			else if(vk.hat != -1)
			{
				// hat
				vk.down = SDL_JoystickGetHat(p_joystick, vk.hat) == vk.hatDir;
			}
		}
	}
}

void Engine::clearActionEdges()
{
	for(std::unordered_map<std::string, Action*>::const_iterator it = actions.begin();
		it != actions.end();
		++it)
	{
		it->second->data &= ~(2 | 4);
	}
}

void Engine::updateActions()
{
	for(std::unordered_map<std::string, Action*>::const_iterator it = actions.begin();
		it != actions.end();
		++it)
	{
		Action& a = *(it->second);

		int oldData = a.data;
		bool oldDown = oldData & 1;

		bool down = false;
		if(a.primary != -1) down |= virtualKeys[a.primary].down;
		if(a.secondary != -1) down |= virtualKeys[a.secondary].down;

		if(down) a.data |= 1;
		else a.data &= ~1;

		if(down && !oldDown)
		{
			// pressed
			if(!a.countDown)
			{
				a.data |= 2;
				// No repeat, no lockout either: otherwise a second press
				// within delay would not count at all, because it would land
				// in the buffer below and that is only there for the repeat.
				a.countDown = a.repeats ? a.delay : 0;

				// reset the opposing actions
				for(std::vector<std::string>::const_iterator jt = a.resetsActions.begin();
					jt != a.resetsActions.end();
					++jt)
				{
					Action* p_reset = getAction(*jt);
					if(p_reset && p_reset->data & 1) p_reset->data |= 8;
				}
			}
			else if(a.repeats && a.buffered < 5)
			{
				// buffer it
				++a.buffered;
				if(a.countDown > a.interval) a.countDown = a.interval;
			}
		}
		else if(!down && oldDown)
		{
			// released
			a.data |= 4;
			a.data &= ~8;

			// re-enable the opposing actions
			for(std::vector<std::string>::const_iterator jt = a.resetsActions.begin();
				jt != a.resetsActions.end();
				++jt)
			{
				Action* p_reset = getAction(*jt);
				if(p_reset && p_reset->data & 1) p_reset->data &= ~8;
			}
		}
		else if(down && oldDown)
		{
			// down, and down in the previous tick too
			if(a.repeats && !a.countDown)
			{
				a.data |= 2;
				a.countDown += a.interval;
			}
		}

		if(a.countDown)
		{
			a.countDown -= logicRate;
			if(a.countDown <= 0)
			{
				a.countDown = 0;

				// Is anything still buffered?
				if(a.buffered)
				{
					--a.buffered;

					a.data = 1 | 2 | 4;
					a.countDown = a.interval;

					// reset the opposing actions
					for(std::vector<std::string>::const_iterator jt = a.resetsActions.begin();
						jt != a.resetsActions.end();
						++jt)
					{
						Action* p_reset = getAction(*jt);
						if(p_reset && p_reset->data & 1) p_reset->data |= 8;
					}
				}
			}
		}
	}
}

void Engine::flushInput()
{
	// First get rid of what has piled up - but only keys and mouse. Everything
	// else has to stay, above all SDL_VIDEORESIZE.
	// SDL_PeepEvents takes different things: SDL 1.2 a bitmask, Emscripten's
	// reimplementation the SDL 2 shape - and there only *one* event per call.
	SDL_Event events[32];
	SDL_PumpEvents();
#ifdef __EMSCRIPTEN__
	while(SDL_PeepEvents(events, 1, SDL_GETEVENT, SDL_KEYDOWN, SDL_KEYUP) > 0) {}
	while(SDL_PeepEvents(events, 1, SDL_GETEVENT, SDL_MOUSEMOTION, SDL_MOUSEBUTTONUP) > 0) {}
#else
	while(SDL_PeepEvents(events, 32, SDL_GETEVENT,
						 SDL_EVENTMASK(SDL_KEYDOWN) |
						 SDL_EVENTMASK(SDL_KEYUP) |
						 SDL_EVENTMASK(SDL_MOUSEBUTTONDOWN) |
						 SDL_EVENTMASK(SDL_MOUSEBUTTONUP) |
						 SDL_EVENTMASK(SDL_MOUSEMOTION)) > 0) {}
#endif

	// ... then our own state and its flags. If a mouse button stayed down, the
	// GUI would read the next release as a click.
	for(int i = 0; i < NUM_KEY_SLOTS; i++)
	{
		keyData[i] = 0;
		buttonData[i] = 0;
	}
	while(!keyEventQueue.empty()) keyEventQueue.pop();

	// Do not clear keyHeld; bring it up to date. Either alternative would be
	// wrong: a release can be among the discarded events, and then the key
	// would stay held for ever - while clearing it outright makes a held key
	// look like a fresh press at the next event. During a key grab this runs
	// every tick, where it shows up at once. So the keyboard itself is asked.
	//
	// SDL says how long its array is: NUM_KEY_SLOTS is SDLK_LAST, so 1536 under
	// Emscripten's headers, while SDL 2 sits underneath there and its array
	// only reaches SDL_NUM_SCANCODES.
	int numKeys = 0;
#ifdef __EMSCRIPTEN__
	Uint8* p_keys = SDL_GetKeyboardState(&numKeys);
#else
	Uint8* p_keys = SDL_GetKeyState(&numKeys);
#endif
	if(numKeys > NUM_KEY_SLOTS) numKeys = NUM_KEY_SLOTS;
	for(int i = 0; i < numKeys; i++) keyHeld[i] = p_keys[i] != 0;
	for(int i = numKeys; i < NUM_KEY_SLOTS; i++) keyHeld[i] = false;
}

void Engine::showLastFrame()
{
	unbindFrameBuffer();
	presentFrame();
	SDL_GL_SwapBuffers();
}

void Engine::beginKeyGrab(int timeOutMS)
{
	// What is already down now does not count: the key wanted is the one that
	// goes down anew while waiting.
	updateVKs();

	grabOldState.clear();
	grabOldState.reserve(virtualKeys.size());
	for(size_t i = 0; i < virtualKeys.size(); i++) grabOldState.push_back(virtualKeys[i].down);

	grabHasDeadline = timeOutMS > 0;
	grabDeadline = SDL_GetTicks() + static_cast<uint>(timeOutMS > 0 ? timeOutMS : 0);
	grabResult = GRAB_WAITING;
	grabbingKey = true;
}

bool Engine::isGrabbingKey() const
{
	return grabbingKey;
}

int Engine::pollKeyGrab()
{
	if(grabbingKey) return GRAB_WAITING;

	const int result = grabResult;
	grabResult = GRAB_WAITING;
	return result;
}

void Engine::updateKeyGrab()
{
	if(!grabbingKey) return;

	// Escape is not offered as a binding. It means "no key" and clears the
	// binding away - the only way to leave an action unbound.
	if(virtualKeys[getKeyboardVK(SDLK_ESCAPE)].down)
	{
		grabResult = GRAB_NO_KEY;
		grabbingKey = false;
		return;
	}

	const size_t n = min(grabOldState.size(), virtualKeys.size());
	for(size_t i = 0; i < n; i++)
	{
		if(virtualKeys[i].down && !grabOldState[i])
		{
			grabResult = static_cast<int>(i);
			grabbingKey = false;
			return;
		}
	}

	// Time is up and nothing was pressed, so nothing was asked for: the old
	// binding stays. Waiting is what somebody does who opened this by accident
	// or thought better of it, and it must not cost them the key they had.
	if(grabHasDeadline && SDL_GetTicks() >= grabDeadline)
	{
		grabResult = GRAB_TIMED_OUT;
		grabbingKey = false;
	}
}

void Engine::resetActions()
{
	for(size_t i = 0; i < actionsVector.size(); i++)
	{
		resetAction(actionsVector[i]->name);
	}
}

void Engine::resetAction(const std::string& name)
{
	Action* p_action = getAction(name);
	if(!p_action) return;

	p_action->primary = p_action->defaultPrimary;
	p_action->secondary = p_action->defaultSecondary;
	syncActionDown(*p_action);
}

// Picks up whether the bound keys are currently down, without producing an
// edge. updateVKs() must have run in this tick.
void Engine::syncActionDown(Action& action)
{
	const int count = static_cast<int>(virtualKeys.size());
	bool down = false;
	if(action.primary   >= 0 && action.primary   < count) down = down || virtualKeys[action.primary].down;
	if(action.secondary >= 0 && action.secondary < count) down = down || virtualKeys[action.secondary].down;

	if(down) action.data |= 1;
	else     action.data &= ~1;
}

// Turn the ids remembered while loading into indices. Must run once
// virtualKeys stands. An id that does not resolve - a joystick that is not
// connected - stays unbound.
void Engine::resolveActionKeys()
{
	for(size_t i = 0; i < actionsVector.size(); i++)
	{
		Action& a = *actionsVector[i];

		if(!a.pendingPrimaryId.empty())
		{
			a.primary = getVKFromId(a.pendingPrimaryId);
			a.pendingPrimaryId.clear();
		}

		if(!a.pendingSecondaryId.empty())
		{
			a.secondary = getVKFromId(a.pendingSecondaryId);
			a.pendingSecondaryId.clear();
		}
	}
}

// If truly *not one* action is bound, that cannot be deliberate: then the
// defaults apply again. A game with no binding at all cannot be operated, and
// the way out - Options, Reset - is not apparent to anybody.
void Engine::repairLostBindings()
{
	if(actionsVector.empty()) return;

	for(size_t i = 0; i < actionsVector.size(); i++)
	{
		if(actionsVector[i]->primary != -1 || actionsVector[i]->secondary != -1) return;
	}

	printfLog("+ WARNING: No action had a key assigned; restoring the defaults.\n");
	resetActions();
}

void Engine::limitActionKeys()
{
	// limit the actions' indices
	for(std::unordered_map<std::string, Action*>::const_iterator it = actions.begin();
		it != actions.end();
		++it)
	{
		Action& a = *(it->second);
		if(a.primary >= static_cast<int>(virtualKeys.size())) a.primary = -1;
		if(a.secondary >= static_cast<int>(virtualKeys.size())) a.secondary = -1;
	}
}

Vec2i Engine::getCursorPosition() const
{
	Vec2i position = cursorPosition;

	if(useFrameBuffer)
	{
		// Exactly the inverse of what presentFrame() draws. The rectangle is
		// centred, so the arithmetic holds in SDL's window coordinates as
		// well as in GL's. Computed with pixel centres; only that makes the
		// round trip exact.
		int x, y, w, h;
		computePresentRect(x, y, w, h);
		if(w > 0 && h > 0)
		{
			Vec2d n((position.x + 0.5 - x) / w, (position.y + 0.5 - y) / h);

			// The same curvature as in the shader: the cursor sits on the glass.
			// Without curvature warpToSource returns the coordinate unchanged.
			const Vec2d warped = warpToSource(n * 2.0 - Vec2d(1.0, 1.0));
			n = (warped + Vec2d(1.0, 1.0)) * 0.5;

			position.x = static_cast<int>(floor(n.x * screenSize.x));
			position.y = static_cast<int>(floor(n.y * screenSize.y));
		}
	}

	position = Vec2i(clamp(position.x, 0, screenSize.x - 1),
					 clamp(position.y, 0, screenSize.y - 1));

	return position;
}

const Vec2i& Engine::getRawCursorPosition() const
{
	return cursorPosition;
}

void Engine::setCursorPosition(const Vec2i& cursorPosition)
{
	// Clamp into the valid range of the internal picture first, then convert
	// outward.
	Vec2i temp = Vec2i(clamp(cursorPosition.x, 0, screenSize.x - 1),
					   clamp(cursorPosition.y, 0, screenSize.y - 1));

	if(useFrameBuffer)
	{
		int x, y, w, h;
		computePresentRect(x, y, w, h);
		if(screenSize.x > 0 && screenSize.y > 0)
		{
			Vec2d n((temp.x + 0.5) / screenSize.x, (temp.y + 0.5) / screenSize.y);

			// The way back through the curvature. With the CRT filter off this
			// is the identity.
			const Vec2d out = warpToOutput(n * 2.0 - Vec2d(1.0, 1.0));
			n = (out + Vec2d(1.0, 1.0)) * 0.5;

			temp.x = x + static_cast<int>(floor(n.x * w));
			temp.y = y + static_cast<int>(floor(n.y * h));
		}
	}

	SDL_WarpMouse(temp.x, temp.y);
	this->cursorPosition = temp;
}

uint Engine::getLogicRate() const
{
	return logicRate;
}

void Engine::setLogicRate(uint logicRate)
{
	this->logicRate = logicRate;
}

uint Engine::getFrameTime() const
{
	return frameTime;
}

uint Engine::getTime() const
{
	return time;
}

const Vec2i& Engine::getScreenSize() const
{
	return screenSize;
}

const Vec2i& Engine::getScreenPow2Size() const
{
	return screenPow2Size;
}

const Vec2i& Engine::getDisplaySize() const
{
	return displaySize;
}

void Engine::crossfade(Crossfade* p_crossfade,
					   double duration,
					   bool immediately)
{
	if(!p_crossfade || duration <= 0.0)
	{
		// cancel the crossfade
		delete this->p_crossfade;
		this->p_crossfade = 0;
		crossfadeTime = -1.0;
		crossfadeDuration = 0.0;
	}
	else
	{
		// start the crossfade
		this->p_crossfade = p_crossfade;
		crossfadeTime = -0.51;
		crossfadeDuration = duration;
	}

	if(immediately)
	{
		// save the old image - as above out of the framebuffer object, not out
		// of whatever is bound right now.
		bindFrameBuffer();
		glBindTexture(GL_TEXTURE_2D, oldImageID);
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, screenPow2Size.y - screenSize.y, 0, 0, screenSize.x, screenSize.y);
		crossfadeTime = -0.5;
	}
}

// Tells the page's on-screen pad which language to label its buttons in. A
// static and not a member because it is the memory of one singleton talking to
// one page, and it keeps the string from crossing into JavaScript every tick
// for the whole run. Nothing outside the browser has a pad to tell.
void Engine::publishLanguage()
{
#ifdef __EMSCRIPTEN__
	static std::string published;
	if(language == published) return;
	published = language;

	EM_ASM({
		if(typeof window.b5_setPadLanguage === 'function')
		{
			window.b5_setPadLanguage(UTF8ToString($0));
		}
	}, language.c_str());
#endif
}

std::string Engine::detectSystemLanguage()
{
	// Only "de" or "en". Of the 349 strings in data/languages.txt exactly one
	// has a French body and one a Spanish, so detecting "fr" here would give an
	// English game with a French label.
#if defined(__EMSCRIPTEN__)
	const int german = EM_ASM_INT({
		var list = navigator.languages || [navigator.language || ""];
		for(var i = 0; i < list.length; i++)
		{
			var tag = String(list[i] || "").toLowerCase();
			if(tag.indexOf("de") === 0) return 1;
			if(tag.indexOf("en") === 0) return 0;
		}
		return 0;
	});
	return german ? "de" : "en";
#elif defined(_WIN32)
	// The UI language, not the locale: somebody who uses their Windows in
	// German wants the game in German. GetLocaleInfoA, not ...W - the project
	// is MultiByte.
	const LANGID langId = GetUserDefaultUILanguage();
	char iso[16] = "";
	if(GetLocaleInfoA(MAKELCID(langId, SORT_DEFAULT), LOCALE_SISO639LANGNAME, iso, sizeof(iso)) > 0)
	{
		if(iso[0] == 'd' && iso[1] == 'e') return "de";
		return "en";
	}
	return PRIMARYLANGID(langId) == LANG_GERMAN ? "de" : "en";
#else
	const char* p_env = getenv("LC_ALL");
	if(!p_env || !*p_env) p_env = getenv("LC_MESSAGES");
	if(!p_env || !*p_env) p_env = getenv("LANG");
	return (p_env && p_env[0] == 'd' && p_env[1] == 'e') ? "de" : "en";
#endif
}

void Engine::loadConfig()
{
	// With no <Language> in config.xml the system decides.
	language = detectSystemLanguage();
	soundVolume = musicVolume = 1.0;
	particleDensity = 1.0;
	details = 2;

	TiXmlDocument doc;
	doc.LoadFile(FileSystem::inst().getAppHomeDirectory() + "config.xml");
	if(doc.ErrorId()) return;

	TiXmlElement* p_config = doc.FirstChildElement("Config");
	if(p_config)
	{
		// read the language
		TiXmlElement* p_language = p_config->FirstChildElement("Language");
		if(p_language)
		{
			const char* p_text = p_language->GetText();
			if(p_text) setLanguage(p_text);
		}
		else printfLog("  No <Language> in config.xml; using the system language: %s\n", language.c_str());

		// Read the upscaling filter. Whether it really works is decided later
		// by getEffectiveUpscaler() - there is no GL context here.
		TiXmlElement* p_upscaler = p_config->FirstChildElement("Upscaler");
		if(p_upscaler)
		{
			Upscaler* p_found = findUpscaler(p_upscaler->GetText());
			// Not an error but an older config.xml: the names were different up
			// to 1.2.0. It is reported all the same - otherwise the filter would
			// one day simply be another one, with nothing anywhere saying so.
			if(p_found) p_wantedUpscaler = p_found;
			else printfLog("  Unknown <Upscaler> \"%s\" in config.xml; using %s.\n",
						   p_upscaler->GetText() ? p_upscaler->GetText() : "",
						   p_wantedUpscaler->getName());
		}

		// And whatever the filters themselves have to set - each reads its own
		// element, even when it is not the chosen one.
		for(std::vector<Upscaler*>::iterator i = upscalers.begin(); i != upscalers.end(); ++i)
		{
			(*i)->loadConfig(p_config);
		}

		// The window: position, size, maximized, fullscreen. All four apply at
		// the next start - during play the player switches for themselves.
		TiXmlElement* p_window = p_config->FirstChildElement("Window");
		if(p_window)
		{
			// Negative values are allowed: a second screen to the left of the
			// first has them. restoreWindowPosition() checks the spot.
			int x = 0, y = 0;
			const bool haveX = p_window->QueryIntAttribute("positionX", &x) == TIXML_SUCCESS;
			const bool haveY = p_window->QueryIntAttribute("positionY", &y) == TIXML_SUCCESS;
			if(haveX && haveY && abs(x) <= 32768 && abs(y) <= 32768)
			{
				windowedPosition = Vec2i(x, y);
				windowedPositionKnown = true;
			}

			int w = 0, h = 0;
			p_window->QueryIntAttribute("sizeX", &w);
			p_window->QueryIntAttribute("sizeY", &h);
			// Smaller than the internal picture makes no sense, and neither
			// does an absurdly large number out of a mangled file.
			if(w >= screenSize.x && h >= screenSize.y && w <= 16384 && h <= 16384)
				windowedSize = Vec2i(w, h);

			int value = 0;
			p_window->QueryIntAttribute("maximized", &value);
			maximized = (value != 0);

			// In the browser there is no window to put back up at the next
			// start: the page decides, and the fullscreen needs a real gesture
			// anyway. It is written all the same, so that the same file is the
			// same on both sides.
#ifndef __EMSCRIPTEN__
			value = 0;
			if(p_window->QueryIntAttribute("fullscreen", &value) == TIXML_SUCCESS)
				fullScreen = (value != 0);
#endif
		}

		// read the sound volume
		TiXmlElement* p_soundVolume = p_config->FirstChildElement("SoundVolume");
		if(p_soundVolume)
		{
			const char* p_text = p_soundVolume->GetText();
			if(p_text) setSoundVolume(atof(p_text));
		}

		// read the music volume
		TiXmlElement* p_musicVolume = p_config->FirstChildElement("MusicVolume");
		if(p_musicVolume)
		{
			const char* p_text = p_musicVolume->GetText();
			if(p_text) setMusicVolume(atof(p_text));
		}

		// read the details
		TiXmlElement* p_details = p_config->FirstChildElement("Details");
		if(p_details)
		{
			const char* p_text = p_details->GetText();
			if(p_text) setDetails(atoi(p_text));
		}

		// read the controls
		TiXmlElement* p_controls = p_config->FirstChildElement("Controls");
		if(p_controls)
		{
			TiXmlElement* p_action = p_controls->FirstChildElement("Action");
			while(p_action)
			{
				const char* p_name = p_action->Attribute("name");
				if(p_name)
				{
					if(getAction(p_name))
					{
						// Try the number first: if it works,
						// this is a file from before 1.2.0. A
						// name is only remembered here, because
						// virtualKeys does not stand yet -
						// resolveActionKeys() fills it in.
						int primary = -1, secondary = -1;
						const char* p_primaryId = p_action->Attribute("primary");
						const char* p_secondaryId = p_action->Attribute("secondary");

						Action* p_theAction = getAction(p_name);
						if(p_action->QueryIntAttribute("primary", &primary) != TIXML_SUCCESS)
							p_theAction->pendingPrimaryId = p_primaryId ? p_primaryId : "";
						if(p_action->QueryIntAttribute("secondary", &secondary) != TIXML_SUCCESS)
							p_theAction->pendingSecondaryId = p_secondaryId ? p_secondaryId : "";

						changeAction(p_name, primary, secondary);
					}
				}

				p_action = p_action->NextSiblingElement("Action");
			}
		}
		else
		{
			resetActions();
		}
	}

	// If loadConfig() is ever called when the list already stands, the
	// follow-up is due at once.
	if(!virtualKeys.empty())
	{
		resolveActionKeys();
		limitActionKeys();
		repairLostBindings();
	}
}

void Engine::saveConfig()
{
	TiXmlDocument doc;

	TiXmlDeclaration* p_decl = new TiXmlDeclaration("1.0", "", "");
	doc.LinkEndChild(p_decl);

	TiXmlElement* p_config = new TiXmlElement("Config");

	// write the language
	TiXmlElement* p_language = new TiXmlElement("Language");
	p_language->LinkEndChild(new TiXmlText(language));
	p_config->LinkEndChild(p_language);

	// Write the upscaling filter - the wish, not what this machine makes of it.
	TiXmlElement* p_upscaler = new TiXmlElement("Upscaler");
	p_upscaler->LinkEndChild(new TiXmlText(p_wantedUpscaler->getName()));
	p_config->LinkEndChild(p_upscaler);

	// And straight after it whatever the filters themselves have to set. Each
	// creates its own element; all that stands here is the order.
	for(std::vector<Upscaler*>::iterator i = upscalers.begin(); i != upscalers.end(); ++i)
	{
		(*i)->saveConfig(p_config);
	}

	// Write the window, so that the game comes back the way it left. The
	// position is only there when one is known - on a first start there is
	// none, and a 0,0 would be a claim.
	TiXmlElement* p_window = new TiXmlElement("Window");
	if(windowedPositionKnown)
	{
		p_window->SetAttribute("positionX", windowedPosition.x);
		p_window->SetAttribute("positionY", windowedPosition.y);
	}
	p_window->SetAttribute("sizeX", windowedSize.x);
	p_window->SetAttribute("sizeY", windowedSize.y);
	p_window->SetAttribute("maximized", maximized ? 1 : 0);
	p_window->SetAttribute("fullscreen", fullScreen ? 1 : 0);
	p_config->LinkEndChild(p_window);

	// write the sound volume
	TiXmlElement* p_soundVolume = new TiXmlElement("SoundVolume");
	char temp[256] = "";
	sprintf(temp, "%f", getSoundVolume());
	p_soundVolume->LinkEndChild(new TiXmlText(temp));
	p_config->LinkEndChild(p_soundVolume);

	// write the music volume
	TiXmlElement* p_musicVolume = new TiXmlElement("MusicVolume");
	sprintf(temp, "%f", getMusicVolume());
	p_musicVolume->LinkEndChild(new TiXmlText(temp));
	p_config->LinkEndChild(p_musicVolume);

	// write the details
	TiXmlElement* p_details = new TiXmlElement("Details");
	sprintf(temp, "%d", getDetails());
	p_details->LinkEndChild(new TiXmlText(temp));
	p_config->LinkEndChild(p_details);

	// write the controls
	TiXmlElement* p_controls = new TiXmlElement("Controls");
	for(size_t i = 0; i < actionsVector.size(); i++)
	{
		TiXmlElement* p_action = new TiXmlElement("Action");
		p_action->SetAttribute("name", actionsVector[i]->name.c_str());
		p_action->SetAttribute("primary", getVKId(actionsVector[i]->primary).c_str());
		p_action->SetAttribute("secondary", getVKId(actionsVector[i]->secondary).c_str());
		p_controls->LinkEndChild(p_action);
	}
	p_config->LinkEndChild(p_controls);

	doc.LinkEndChild(p_config);

	doc.SaveFile(FileSystem::inst().getAppHomeDirectory() + "config.xml");
}

const std::string& Engine::getLanguage() const
{
	return language;
}

void Engine::setLanguage(const std::string& language)
{
	if(language != "de" && language != "en") return;
	this->language = language;
}

double Engine::getSoundVolume() const
{
	return soundVolume;
}

void Engine::setSoundVolume(double soundVolume)
{
	soundVolume = clamp(soundVolume, 0.0, 1.0);

	this->soundVolume = soundVolume;
	volumeChanged = true;
}

double Engine::getMusicVolume() const
{
	return musicVolume;
}

void Engine::setMusicVolume(double musicVolume)
{
	musicVolume = clamp(musicVolume, 0.0, 1.0);

	this->musicVolume = musicVolume;
	volumeChanged = true;
}

bool Engine::wasVolumeChanged() const
{
	return volumeChanged;
}

bool Engine::isAppActive() const
{
	return appActive;
}

int Engine::getDetails() const
{
	return details;
}

void Engine::setDetails(int details)
{
	this->details = details;

	if(details == 0) setParticleDensity(0.333);
	else if(details == 1) setParticleDensity(0.666);
	else setParticleDensity(1.0);
}

double Engine::getParticleDensity() const
{
	return particleDensity;
}

void Engine::setParticleDensity(double particleDensity)
{
	particleDensity = clamp(particleDensity, 0.0, 1.0);

	this->particleDensity = particleDensity;
}

void Engine::setMuteIcon(Texture* p_texture,
						 const Vec2i& positionOnTexture,
						 const Vec2i& size)
{
	p_muteIconTexture = p_texture;
	muteIconPositionOnTexture = positionOnTexture;
	muteIconSize = size;
}

void Engine::setRecordingIcon(Texture* p_texture,
							  const Vec2i& positionOnTexture,
							  const Vec2i& size)
{
	p_recordingIconTexture = p_texture;
	recordingIconPositionOnTexture = positionOnTexture;
	recordingIconSize = size;
}

void Engine::loadSoundVolumes(const std::string& filename)
{
	soundVolumes.clear();

	// Through the virtual filesystem and not with TiXmlDocument::LoadFile: the
	// file sits in the encrypted data.zip, and LoadFile only knows stdio.
	const std::string text = FileSystem::inst().readStringFromFile(filename);
	TiXmlDocument doc;
	doc.Parse(text.c_str());

	TiXmlElement* p_root = doc.RootElement();
	if(!p_root)
	{
		// No reason to give up: without the table every sound plays at 1.0, and
		// that is right for all but a handful anyway.
		printfLog("+ ERROR: Could not read \"%s\".\n", filename.c_str());
		return;
	}

	for(TiXmlElement* p_elem = p_root->FirstChildElement("Sound"); p_elem;
		p_elem = p_elem->NextSiblingElement("Sound"))
	{
		const char* p_file = p_elem->Attribute("file");
		double volume = 1.0;
		if(p_file && p_elem->Attribute("volume", &volume)) soundVolumes[p_file] = volume;
	}

	printfLog("* %u sound volume(s) read from \"%s\".\n",
			  static_cast<uint>(soundVolumes.size()), filename.c_str());
}

double Engine::getSoundVolumeFactor(const std::string& filename) const
{
	const std::unordered_map<std::string, double>::const_iterator i =
		soundVolumes.find(filename);
	return (i == soundVolumes.end()) ? 1.0 : i->second;
}

void Engine::loadStringDB(const std::string& filename)
{
	std::string file = FileSystem::inst().readStringFromFile(filename);

	std::string line;
	std::string id;
	std::string texts;
	int numEmptyLines = 0;
	bool dontCollapse = false;

	for(uint i = 0; i < file.length(); i++)
	{
		const char c = file[i];
		if(c == '\r' || c == '\n')
		{
			// The line is finished!

			if(line.empty())
			{
				// An empty line is stored unless it stands at the beginning.
				if(!texts.empty()) numEmptyLines++;
			}
			else if(line.compare(0, 2, "//") == 0)
			{
				// It is only a comment.
			}
			else
			{
				if(line[0] == '$')
				{
					// This is the string ID!

					// store the old string first
					stringDB[id] = texts;

					// start over
					id = line;

					dontCollapse = false;
					if(id[id.length() - 1] == '#')
					{
						dontCollapse = true;
						id.resize(id.length() - 1);
					}

					texts = "";
					numEmptyLines = 0;
				}
				else
				{
					// This is a text line!
					if(texts.empty()) texts = line;
					else
					{
						if(line[0] == '\xA7')
						{
							texts += std::string(dontCollapse ? "\n" : "") + line;
							numEmptyLines = 0;
						}
						else
						{
							if(numEmptyLines)
							{
								texts.append(numEmptyLines, '\n');
								numEmptyLines = 0;
							}

							texts += std::string("\n") + line;
						}
					}
				}
			}

			line = "";

			// skip the \n after a \r
			if(c == '\r') i++;
		}
		else
		{
			line.append(1, c);
		}
	}
}

std::string Engine::localizeString(const std::string& text)
{
	return expandBindings(localizeStringRaw(text));
}

std::string Engine::localizeStringRaw(const std::string& text)
{
	if(!text.empty())
	{
		if(text[0] == '$')
		{
			// Is this string in the database?
			std::unordered_map<std::string, std::string>::const_iterator i = stringDB.find(text);
			if(i != stringDB.end())
			{
				// Yes! Localize it!
				return localizeStringRaw(i->second);
			}
		}
	}

	// generate the search pattern
	const std::string patternStart = std::string("\xA7") + language + std::string(":");

	std::string::size_type indexStart = text.find(patternStart);
	if(std::string::npos == indexStart)
	{
		// No localization for this language!
		if(language == "en")
		{
			// return the string unchanged
			return text;
		}
		else
		{
			// try again in English ...
			std::string oldLanguage = language;
			language = "en";
			std::string result = localizeStringRaw(text);
			language = oldLanguage;
			return result;
		}
	}

	std::string::size_type textStart = indexStart + language.length() + 2;

	std::string::size_type textEnd = text.find("\xA7", textStart);
	if(std::string::npos == textEnd)
	{
		// This is the last localization.
		return text.substr(textStart);
	}

	return text.substr(textStart, textEnd - textStart);
}

std::string Engine::getVKDisplayName(int vk)
{
	if(vk < 0 || vk >= static_cast<int>(virtualKeys.size()))
		return localizeStringRaw("$O_NOT_ASSIGNED");

	const VirtualKey& key = virtualKeys[vk];

	// A joystick keeps its device word, because "B3" beside a keyboard key
	// would say nothing about where it is. The word is localized and the
	// number is not, so the two are put together here rather than in the table.
	if(key.device >= 0)
		return localizeStringRaw("$VK_JOYSTICK") + " " + key.niceName;

	return localizeStringRaw(key.niceName);
}

std::string Engine::getBindingMarkup(const std::string& actionName)
{
	const Action* p_action = getAction(actionName);

	std::string out;
	if(p_action)
	{
		// Either may be unbound on its own, so this is not "both or neither".
		if(p_action->primary >= 0) out = "<k>" + getVKDisplayName(p_action->primary) + "</k>";
		if(p_action->secondary >= 0)
		{
			// Half spaces around the slash: each keycap already stands off its
			// own frame, so a full space either side leaves the slash adrift
			// between the two keys instead of between two words.
			if(!out.empty()) { out += HALF_SPACE; out += '/'; out += HALF_SPACE; }
			out += "<k>" + getVKDisplayName(p_action->secondary) + "</k>";
		}
	}

	if(out.empty()) out = "<k>" + localizeStringRaw("$O_NOT_ASSIGNED") + "</k>";
	return out;
}

std::string Engine::expandBindings(const std::string& text)
{
	// Almost every string in the game has none of this, and localizeString()
	// runs on every caption of every frame.
	if(text.find('%') == std::string::npos) return text;

	// The longer name has to be tested first: it begins with the shorter one.
	static const char* const p_optional = "%BINDING_OPTIONAL_RIGHT{";
	static const char* const p_plain = "%BINDING{";

	std::string out;
	size_t i = 0;
	while(i < text.length())
	{
		bool optional = text.compare(i, strlen(p_optional), p_optional) == 0;
		bool plain = !optional && text.compare(i, strlen(p_plain), p_plain) == 0;
		if(!optional && !plain)
		{
			out += text[i++];
			continue;
		}

		const size_t open = i + strlen(optional ? p_optional : p_plain);
		const size_t close = text.find('}', open);
		if(close == std::string::npos)
		{
			// Unterminated, so it is not markup after all - it stays as it is
			// rather than swallowing the rest of the sentence.
			out += text[i++];
			continue;
		}

		const std::string action = text.substr(open, close - open);
		const Action* p_action = getAction(action);
		const bool bound = p_action && (p_action->primary >= 0 || p_action->secondary >= 0);

		// The optional form is for a caption that names its own shortcut: with
		// nothing bound there is no shortcut to name, and the space in front of
		// it would otherwise be left hanging at the end of the word.
		if(optional) { if(bound) out += " " + getBindingMarkup(action); }
		else out += getBindingMarkup(action);

		i = close + 1;
	}

	return out;
}

std::string Engine::loadString(const std::string& id) const
{
	std::unordered_map<std::string, std::string>::const_iterator i = stringDB.find(id);
	if(i == stringDB.end()) return id;
	else return i->second;
}

AudioCapture* Engine::getAudioCapture()
{
	return p_audioCapture;
}

void Engine::setupCursor()
{
	// The arrow, at the size it is designed for. What the system draws is built
	// from it: once as it stands and once doubled pixel by pixel.
	const char* p_arrow[] = {
		"X               ",
		"XX              ",
		"X.X             ",
		"X..X            ",
		"X...X           ",
		"X....X          ",
		"X.....X         ",
		"X......X        ",
		"X.......X       ",
		"X.....XXX       ",
		"X..X..X         ",
		"X.X X..X        ",
		"XX  X..X        ",
		"     X..X       ",
		"     X..X       ",
		"      XX        "
	};

	for(int row = 0; row < 16; ++row)
	{
		for(int col = 0; col < 16; ++col)
		{
			switch(p_arrow[row][col])
			{
			case 'X': cursorImage[row][col] =  0; break;
			case '.': cursorImage[row][col] =  1; break;
			default:  cursorImage[row][col] = -1; break;
			}
		}
	}

	p_cursor1x = createCursor(1);
	p_cursor2x = createCursor(2);
	cursorScale = 0;
	updateCursorSize();
}

SDL_Cursor* Engine::createCursor(int factor) const
{
	// SDL wants two bitmasks, one bit per pixel and the most significant first:
	// data says black or white, mask says visible or transparent.
	const int size = 16 * factor;
	Uint8 data[4 * 32];
	Uint8 mask[4 * 32];

	int i = -1;
	for(int row = 0; row < size; ++row)
	{
		for(int col = 0; col < size; ++col)
		{
			if(col % 8)
			{
				data[i] <<= 1;
				mask[i] <<= 1;
			}
			else
			{
				++i;
				data[i] = mask[i] = 0;
			}

			const int color = cursorImage[row / factor][col / factor];
			if(color == 0) { data[i] |= 0x01; mask[i] |= 0x01; }
			else if(color == 1) { mask[i] |= 0x01; }
		}
	}

	// The tip is in the corner, at every size.
	return SDL_CreateCursor(data, mask, size, size, 0, 0);
}

void Engine::updateCursorSize()
{
	// The system draws the cursor in window pixels while the game's picture is
	// scaled - so the right size hangs on how large the picture currently
	// stands on the screen. Measured against the rectangle presentFrame()
	// fills and not against the window: Sharp snaps to whole scale steps, and
	// then the picture is smaller than the window.
	//
	// Called once per frame from render(); only changed when something really
	// changes.
	//
	// There are 16 and 32 pixels to choose from - no cursor takes anything
	// larger. At a scale s, 16*s would be right, so the one landing closer to
	// it is taken: |32 - 16s| < |16 - 16s| holds from s = 1.5. At exactly 1 and
	// exactly 2 - where almost everyone sits - the chosen one lines up with the
	// picture pixel for pixel too.
	int x, y, w, h;
	computePresentRect(x, y, w, h);
	const double scale = screenSize.x ? static_cast<double>(w) / screenSize.x : 1.0;

	const int wanted = scale >= 1.5 ? 2 : 1;
	if(wanted == cursorScale) return;

	SDL_Cursor* p_cursor = wanted == 2 ? p_cursor2x : p_cursor1x;
	if(!p_cursor) return;

	cursorScale = wanted;
	SDL_SetCursor(p_cursor);
}
