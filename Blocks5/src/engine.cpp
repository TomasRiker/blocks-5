#include "pch.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#include "web_audio.h"
// Defined further down, next to setFullScreen().
static EM_BOOL engineFullScreenHotkey(int, const EmscriptenKeyboardEvent*, void*);
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
#include "fatalerror.h"
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
const float MASTER_HEADROOM = 0.45f;

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

	time = 0;
	dragButtons = 0;
	dragAxis = -1;
	dragBlocked = false;
	grabbingKey = false;
	grabResult = GRAB_WAITING;
	grabDeadline = 0;
	grabHasDeadline = false;
	p_crossfade = 0;
	p_display = 0;
	p_audioDevice = 0;
	p_audioContext = 0;
	p_currentMusic = 0;
	p_stateToBeEntered = 0;
	p_stateToGetFocus = 0;
	p_stateToLoseFocus = 0;
	p_recordingIconTexture = 0;
	crossfadeTime = -1.0f;
	crossfadeDuration = 0.0f;
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
	presentVertexBuffer = 0;
	// The four filters. They stand before loadConfig(), which looks one of them
	// up by name, and that is long before the GL context; their GL state comes
	// into being only in createUpscalerGL(). The order is the options dialog's:
	// the best first, the matter of style last.
	p_sharpFit = new U_SharpFit();
	p_crt      = new U_Crt();
	upscalers.push_back(p_sharpFit);
	upscalers.push_back(new U_Sharp());
	upscalers.push_back(new U_Smooth());
	upscalers.push_back(p_crt);
	p_wantedUpscaler = p_sharpFit;
	fullScreen = false;
	fullScreenOverride = -1;
	splashSkipped = false;
	performanceShown = false;
	renderDraws = 0;
	renderedFrames = 0;
	sceneTick = 0;
	lastFrameBegin = 0;
	swallowedReturn = false;
	windowedSize = Vec2i(0, 0);      // 0 = nothing chosen yet, init() decides
	windowedPosition = Vec2i(0, 0);
	windowedPositionKnown = false;
	maximized = false;
#ifdef _WIN32
	inSizeMove = false;
#endif
	savedWindowStyle = 0;
	muted = false;
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

	// The six keys of the mouse drag, and they sit exactly here for a reason:
	// main.cpp registers its actions before Engine::init runs, so a binding
	// has to be nameable before this table exists. A keyboard key manages it
	// because its index is its own key code (getKeyboardVK), and these manage
	// it by following the keyboard block immediately - the loop above has just
	// pushed SDLK_LAST of them - so the base is SDLK_LAST whatever joysticks
	// turn up afterwards. getMouseDragVK() returns that without reading this.
	//
	// updateMouseDrag() sets them from the cursor and the buttons, the way a
	// joystick hat's four are polled, so that nothing above the input layer
	// learns a mouse can steer a character. The ids are structural, as the
	// joysticks' are, and carry no $ID because nothing shows them: the drag is
	// bound as an action's third source, which the options dialog does not
	// offer - a gesture is its own binding.
	{
		static const char* p_ids[NUM_MOUSE_DRAG_VKS] =
			{"Mouse DragW", "Mouse DragE", "Mouse DragN", "Mouse DragS",
			 "Mouse DragB2", "Mouse DragB12"};
		static const char* p_names[NUM_MOUSE_DRAG_VKS] =
			{"Mouse drag left", "Mouse drag right", "Mouse drag up",
			 "Mouse drag down", "Mouse drag, right button",
			 "Mouse drag, both buttons"};
		for(int i = 0; i < NUM_MOUSE_DRAG_VKS; i++)
		{
			VirtualKey vk;
			vk.device = VK_DEVICE_MOUSE;
			vk.key = i;
			vk.id = p_ids[i];
			vk.name = p_names[i];
			vk.niceName = p_names[i];
			vk.down = false;
			virtualKeys.push_back(vk);
		}
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

	// A colour buffer and nothing else is asked of the window: the game
	// renders into its framebuffer object, whose texture, depth and stencil
	// are its own, and the window only ever receives the present. Nor is
	// there a GL version to ask for - SDL 1.2 has no such attribute, the
	// context is whatever the driver gives a legacy request, and
	// GLExtensions::init resolves what the game needs out of it and stops
	// where something is missing.
	SDL_GL_SetAttribute(SDL_GL_BUFFER_SIZE, 32);
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
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
	// The first gesture of all, and the pad's button, are the page's own
	// business - pre.js and touch_controls.js.
	emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, 0, EM_TRUE,
									engineFullScreenHotkey);
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

	// All four end the program with a message where this machine cannot do
	// what the game is built on - see fatalerror.h.
	GLExtensions::init();
	Renderer::inst().init();
	createFrameBuffer();
	createUpscalerGL();

	// If the game starts in fullscreen, the style change comes now.
	if(fullScreen) applyWindowStyle(true, getDesktopSize());
	printfLog("  Upscaling:        %s\n", p_wantedUpscaler->getName());

	// Only here: setupCursor() ends in updateCursorSize(), which measures the
	// rectangle presentFrame() fills - so the window must have its final size
	// and the filter must be the one that will draw.
	setupCursor();

	// create the textures for crossfading
	oldImageID = createFrameCopyTexture(false, true);
	newImageID = createFrameCopyTexture(false, true);

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
	alListenerf(AL_GAIN, MASTER_HEADROOM);

	printfLog("* Initializing GUI ...\n");
	if(!GUI::inst().init())
	{
		printfLog("+ ERROR: Could not initialize GUI.\n");
		return false;
	}

	Renderer::inst().setBlend(BM_NORMAL);

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

	// delete the crossfade and the textures - before the managers go, since a
	// crossfade may hold a resource of theirs (CF_Rewind's OSD picture)
	crossfade(0, 0.0f);
	Renderer::inst().deleteTexture(oldImageID);
	Renderer::inst().deleteTexture(newImageID);

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
	SDL_FreeCursor(p_cursor1x);
	SDL_FreeCursor(p_cursor2x);
	p_cursor1x = p_cursor2x = 0;
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

	// The effective volumes just changed; the sounds re-read them next tick.
	volumeChanged = true;

	if(gained)
	{
		GameState* p_gs = getGameState();
		if(p_gs) p_gs->onAppGetFocus();
		return;
	}

	// No release arrives after a change of focus. A key left standing as held
	// here would never yield a key press again, and a mouse button left down
	// is worse than that: the drag recogniser reads it every tick, so a button
	// let go of in another window would still be steering a character on the
	// way back. Both are cleared, and updateMouseDrag() ends the drag by
	// itself on the next tick because nothing is held any more.
	for(int i = 0; i < NUM_KEY_SLOTS; i++)
	{
		keyHeld[i] = false;
		buttonData[i] &= ~1;
	}

	GameState* p_gs = getGameState();
	if(p_gs) p_gs->onAppLoseFocus();

	// stop the video recording if one is running
	if(p_videoRecorder)
	{
		delete p_videoRecorder;
		p_videoRecorder = 0;
	}
}

namespace
{
	// A query and no state, so it stands outside a bracket.
	void reportGLError()
	{
		const uint err = glGetError();
		if(err != GL_NO_ERROR)
		{
			printfLog("+ An OpenGL error occured (Error: %d).\n", err);
		}
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
		const uint64 frameBegin = getExactTimeUS();
		float phases[FrameStats::FS_NUM_PHASES];
		for(int i = 0; i < FrameStats::FS_NUM_PHASES; i++) phases[i] = 0.0f;

		// The renderer's own counter, which runs in every build - the draw
		// calls counted at the link are a test-hooks build only. It is
		// cumulative and reset from outside (the test hook does), so what
		// belongs to this iteration is the difference across it.
		const uint drawsAtStart = Renderer::inst().stats().draws;

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
		reportGLError();

		uint err = alGetError();
		if(err != AL_NO_ERROR)
		{
			printfLog("+ An OpenAL error occured (Error: %d).\n", err);
		}

		bool frameRendered = false;

#ifdef BLOCKS5_TEST_HOOKS
		// The frame the harness photographs is rendered once more after the
		// clock stopped, with the engine's own clock pinned to zero for the
		// length of that one render: the caret, the editor's marching ants and
		// the contamination pulse read getTime(), which counts ticks since the
		// program started and so stands at whatever the harness's timing made
		// it. Pinned and not skipped, because a scene's every other clock is
		// already deterministic and the picture should be too.
		const bool frozenFrame = TestHooks::frozenFrameDue();
#else
		const bool frozenFrame = false;
#endif

		// render
		if(appActive && (timeProcessed || frozenFrame))
		{
			const uint64 renderBegin = getExactTimeUS();
			bindFrameBuffer();
#ifdef BLOCKS5_TEST_HOOKS
			const uint realTime = time;
			if(frozenFrame) time = 0;
#endif
			render();
#ifdef BLOCKS5_TEST_HOOKS
			if(frozenFrame) time = realTime;
#endif
			phases[FrameStats::FS_RENDER] = 0.001f * static_cast<float>(getExactTimeUS() - renderBegin);
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
			showLastFrame();

			updateSounds();
			SDL_Delay(50);
			// Or the first frame after the return would report the whole
			// inactive stretch as its interval.
			lastFrameBegin = 0;
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
		const uint64 updateBegin = getExactTimeUS();
		while(timeToProcess >= logicRate)
		{
#ifdef BLOCKS5_TEST_HOOKS
			// Asked before the tick runs, so the clock stops on exactly the
			// tick that was asked for. While it is stopped the harness must
			// still be answered, or it could neither take its picture nor
			// quit the game - but nothing else of the tick happens, and time
			// does not move, so every frame from here on is the same one.
			TestHooks::checkFreeze(sceneTick, p_crossfade ? static_cast<int>(crossfadeTime * 1000.0f) : -1);
			if(TestHooks::frozen())
			{
#ifndef __EMSCRIPTEN__
				TestHooks::pollRequests();
#endif
				timeToProcess = 0;
				break;
			}
#endif
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

#ifdef BLOCKS5_TEST_HOOKS
			// One tick per rendered frame while the harness asks for it, so
			// that a screen which draws its own last frame back into the next
			// one - the credits - sees the same frames on a machine that
			// drops some as on one that drops none. The backlog is thrown
			// away rather than caught up with later, which is the point.
			if(TestHooks::lockstep()) { timeToProcess = 0; break; }
#endif
		}

		phases[FrameStats::FS_UPDATE] = 0.001f * static_cast<float>(getExactTimeUS() - updateBegin);

		if(crossfadeTime == -0.51f)
		{
			// save the old image. The framebuffer has to be bound explicitly
			// for that: without a logic tick nothing is rendered, and then the
			// screen is still bound - which WebGL clears before every frame.
			bindFrameBuffer();
			captureFrame(oldImageID);
			crossfadeTime = -0.5f;
		}
		else if(crossfadeTime >= -0.5f && frameRendered)
		{
			// fetch the current image and draw the crossfade over it
			captureFrame(newImageID);
			p_crossfade->render(max(0.0f, crossfadeTime / crossfadeDuration), oldImageID, newImageID);
		}

		if(timeProcessed)
		{
			// update the crossfade
			if(crossfadeTime >= 0.0f)
			{
				crossfadeTime += 0.001f * timeProcessed;
				if(crossfadeTime > crossfadeDuration)
				{
					// The crossfade is over!
					crossfadeTime = -1.0f;
					crossfadeDuration = 0.0f;
					delete p_crossfade;
					p_crossfade = 0;
				}
			}
			else if(crossfadeTime == -0.5f) crossfadeTime = -0.25f;
			else if(crossfadeTime == -0.25f) crossfadeTime = 0.0f;
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
					readFrame(static_cast<uchar*>(p_inputFrameBuffer));

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
			const uint64 presentBegin = getExactTimeUS();
			unbindFrameBuffer();
			presentFrame();
			const uint64 swapBegin = getExactTimeUS();
			phases[FrameStats::FS_PRESENT] = 0.001f * static_cast<float>(swapBegin - presentBegin);

			// show the rendered frame
			SDL_GL_SwapBuffers();
			phases[FrameStats::FS_SWAP] = 0.001f * static_cast<float>(getExactTimeUS() - swapBegin);
		}

		Uint32 end = SDL_GetTicks();

		// TOTAL stops here and not after the SDL_Delay below: what is wanted
		// is the work, not the waiting. INTERVAL is start to start and so
		// carries the wait with it, which is what makes the two different
		// numbers worth having side by side.
		{
			const uint64 frameEnd = getExactTimeUS();
			phases[FrameStats::FS_TOTAL] = 0.001f * static_cast<float>(frameEnd - frameBegin);
			if(lastFrameBegin > 0)
				phases[FrameStats::FS_INTERVAL] = 0.001f * static_cast<float>(frameBegin - lastFrameBegin);
			// An iteration that rendered nothing drew nothing, and the zero
			// belongs in the ring: the timings of such an iteration are
			// recorded the same way, and a percentile over the frames that
			// happened to render would answer a different question from the
			// one beside it.
			//
			// The counter can also have been reset since the capture - the
			// test hook answers `resetstats` from inside this very iteration -
			// and an unsigned difference across that would put four billion in
			// the ring rather than a number anybody could read past.
			const uint drawsNow = Renderer::inst().stats().draws;
			phases[FrameStats::FS_DRAWS] =
				static_cast<float>(drawsNow >= drawsAtStart ? drawsNow - drawsAtStart : 0);
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
	const float TOAST_SECONDS_OK    = 2.0f;
	const float TOAST_SECONDS_ERROR = 4.0f;
}

void Engine::showToast(ToastType type,
					   const std::string& text,
					   float duration,
					   bool suppressSound)
{
	if(duration <= 0.0f) duration = (type == TOAST_ERROR) ? TOAST_SECONDS_ERROR : TOAST_SECONDS_OK;
	const uint durationMS = static_cast<uint>(duration * 1000.0f);

	// The sound hangs off the click and not off the message: it comes even
	// when the same message already stands and merely stays longer.
	if(type == TOAST_ERROR && !suppressSound) playSound("teleport_failed.ogg", false, 0.0f, 100);

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
	toast.y = -static_cast<float>(TOAST_HEIGHT);
	toast.targetY = 0.0f;
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
		i->targetY = static_cast<float>(slot * TOAST_HEIGHT);
		slot++;
	}
}

void Engine::updateToasts()
{
	if(toasts.empty()) return;

	// A toast gets this far in one tick: one bar height in the time of a fade.
	// A change of slot therefore takes as long as the slide in.
	const float step = static_cast<float>(TOAST_HEIGHT) * logicRate / TOAST_FADE;

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

	Renderer& renderer = Renderer::inst();
	renderer.setBlend(BM_NORMAL);

	// Oldest first: the newer ones then lie on top, and a toast sliding out
	// disappears behind its younger neighbour.
	for(std::list<Toast>::const_iterator i = toasts.begin(); i != toasts.end(); ++i)
	{
		// Fading in and out goes together with the movement: the bar is not
		// fully opaque, and a sudden disappearance would show.
		float alpha = 1.0f;
		if(i->phase == 0) alpha = static_cast<float>(i->phaseTime) / TOAST_FADE;
		else if(i->phase == 2) alpha = 1.0f - static_cast<float>(i->phaseTime) / TOAST_FADE;
		alpha = clamp(alpha, 0.0f, 1.0f);

		const Vec3f color = i->type == TOAST_ERROR ? Vec3f(0.5f, 0.0f, 0.0f) : Vec3f(0.0f, 0.5f, 0.0f);

		renderer.push();
		renderer.translate(0.0f, floorf(i->y + 0.5f));

		const Vec2f corners[4] = {Vec2f(0.0f, 0.0f), Vec2f(640.0f, 0.0f), Vec2f(640.0f, TOAST_HEIGHT), Vec2f(0.0f, TOAST_HEIGHT)};
		const Vec4f top(color.r, color.g, color.b, 0.75f * alpha);
		const Vec4f bottom(color.r, color.g, color.b, 0.9f * alpha);
		const Vec4f colors[4] = {top, top, bottom, bottom};
		renderer.quad(corners, colors);
		renderer.hairline(Vec2f(0.0f, TOAST_HEIGHT), Vec2f(640.0f, TOAST_HEIGHT), Vec4f(0.0f, 0.0f, 0.0f, 0.9f * alpha));

		if(p_font) p_font->renderText(localizeString(i->text), Vec2i(10, 9), Vec4f(1.0f, 1.0f, 1.0f, alpha));

		renderer.pop();
	}
}

// #define PROFILE_ENGINE_RENDER

#ifdef BLOCKS5_TEST_HOOKS
// What B5_SEED asks for, or 0 for "leave the generator alone". Asked once:
// getenv is not free and this sits at the top of two per-tick functions.
static uint testSeed()
{
	static uint seed = 0;
	static bool asked = false;
	if(!asked)
	{
		asked = true;
		const char* p_seed = ::getenv("B5_SEED");
		if(p_seed && *p_seed) seed = static_cast<uint>(atoi(p_seed));
	}
	return seed;
}

// The load's own stream, clear of the two the tick uses. Those are
// 2 * sceneTick and 2 * sceneTick + 1, and a level's clock is an int counting
// milliseconds, so nothing a run reaches comes near this.
const uint LOAD_STREAM = 0x40000000;
#endif

// Declared without a guard so that Level::load can call it without one:
// BLOCKS5_TEST_HOOKS reaches engine.cpp and testhooks.cpp and not level.cpp,
// and in a normal build this is an empty function the loader calls once.
void Engine::seedForLoad()
{
#ifdef BLOCKS5_TEST_HOOKS
	// A level's objects draw from the generator in their constructors - a
	// Diamond's animation phase is random(0, 100000) - and a load happens
	// between ticks, which is exactly where neither per-tick stream reaches.
	// Those draws would otherwise continue a sequence whose length depends on
	// how many frames the machine managed on the way to the load, so the same
	// diamond stood on a different animation frame from one run to the next.
	if(testSeed()) seedRandom(testSeed() * 2 + LOAD_STREAM);
#endif
}

void Engine::render()
{
#ifdef PROFILE_ENGINE_RENDER
	BEGIN_PROFILE(engineRender)
#endif

#ifdef BLOCKS5_TEST_HOOKS
	// One random stream per rendered frame, keyed on the scene's tick and on
	// nothing else. A rendered frame makes draws of its own - the night
	// vision's two noise offsets, a particle's colour - and a slow machine
	// renders fewer frames than it runs ticks (the render is gated on a tick
	// having run, so there is never more than one per tick and there can be
	// fewer), so without this the picture at a given tick depends on how many
	// frames the machine dropped on the way there. The odd half of the pair;
	// update() takes the even one, which keeps the logic one stream per tick
	// whatever the renderer does.
	//
	// sceneTick and not getTime(), because the engine's clock counts from
	// startup and a harness's click lands at whatever tick the machine got to
	// - so two runs would freeze with the level at two different ages. A
	// level's clock starts at zero when the level loads.
	if(testSeed()) seedRandom(testSeed() * 2 + sceneTick * 2 + 1);
#endif

	// Does the mouse cursor still match what is on the screen? Asked once a
	// frame rather than hung off events: the answer changes in more places
	// than one wants to remember - window size, fullscreen, a filter change,
	// the browser's canvas - and asking costs two divisions and a comparison.
	updateCursorSize();

#ifdef BLOCKS5_TEST_HOOKS
	// Every draw call between here and the end of this function, so that the
	// present's own quad and showLastFrame() stay out of the count.
	const uint drawsBefore = TestHooks::drawCalls;
#endif

	Renderer& renderer = Renderer::inst();
	renderer.frameBegin(screenSize);

	// render the GUI
	GUI::inst().render();

	// render the game
	GameState* p_gs = getGameState();
	if(p_gs) p_gs->onRender();

	// display the GUI
	GUI::inst().display();

	// Toasts last: they sit over the GUI and over the editors' panes.
	renderToasts();

	renderer.frameEnd();

#ifdef BLOCKS5_TEST_HOOKS
	renderDraws += TestHooks::drawCalls - drawsBefore;
	renderedFrames++;
#endif

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

#ifdef BLOCKS5_TEST_HOOKS
	// The even half of render()'s pair - see there.
	if(testSeed()) seedRandom(testSeed() * 2 + sceneTick * 2);
#endif

#if defined(BLOCKS5_TEST_HOOKS) && !defined(__EMSCRIPTEN__)
	// Test build only. In the browser JavaScript calls the dump itself;
	// natively there is no such channel - see testhooks.cpp.
	TestHooks::pollRequests();
#endif

	// Nothing needs a texture's decoded pixels after the tick it was loaded in:
	// the two callers that do ask to keep them (the tile set and the level's
	// sprites, for the debris sampling) ask before this runs.
	Texture::freeUnkeptPixels();

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
		muted = !muted;
		volumeChanged = true;
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
	static int stressTicks = 0;
	if(!(stressTicks % 40))
	{
		const char* s[] = {"GS_LevelEditor", "GS_SelectLevel", "GS_CampaignEditor"};
		pushGameState(s[randomInt() % 3]);
	}
	else if(!((stressTicks + 20) % 40)) popGameState();
	stressTicks++;
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
	Renderer::DirectGL direct;
	// WebGL forbids vertex data out of application memory, it has to be a
	// buffer. Four vertices, refilled every frame; every filter that uses a
	// shader shares this one.
	glExtGenBuffers(1, &presentVertexBuffer);
	if(!presentVertexBuffer)
	{
		fatalError("Blocks 5 - graphics error",
				   "The graphics driver would not create a vertex buffer.\n\n"
				   "There is nothing to be done about this from here; a driver\n"
				   "update is the thing to try.");
	}

	// Four filters, each a program: two bring a fragment shader of their own,
	// two draw through the base class's pass-through. A driver that resolved
	// every GL 2.0 entry point and then will not compile one of them is
	// broken rather than old, so this ends the program as well - leaving a
	// filter out instead is what the whole of the rest of this file no
	// longer has to reckon with.
	for(std::vector<Upscaler*>::iterator i = upscalers.begin(); i != upscalers.end(); ++i)
	{
		if((*i)->createGL()) continue;

		fatalError("Blocks 5 - graphics error",
				   std::string("The \"") + (*i)->getName() + "\" display filter would not compile.\n\n"
				   "log.txt, in the folder with your saved games, has the\n"
				   "compiler's own message. A driver update is the thing to try.");
	}
}

void Engine::destroyUpscalerGL()
{
	Renderer::DirectGL direct;
	for(std::vector<Upscaler*>::iterator i = upscalers.begin(); i != upscalers.end(); ++i)
	{
		(*i)->destroyGL();
	}
	if(presentVertexBuffer) { glExtDeleteBuffers(1, &presentVertexBuffer); presentVertexBuffer = 0; }
}

void Engine::setUpscaler(Upscaler* p_upscaler)
{
	// Never null: every filter works on every machine the game starts on, and
	// a name config.xml does not know leaves the one standing.
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

void Engine::createFrameBuffer()
{
	frameTextureSize = screenPow2Size;
	frameTextureID = Texture::createGLTexture(frameTextureSize, 0, true, true, true);

	Renderer::DirectGL direct;
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
		char detail[64];
		sprintf(detail, "status 0x%x", static_cast<unsigned>(status));
		fatalError("Blocks 5 - graphics error",
				   std::string("The graphics driver would not give the game a render\n"
				   "target to draw into (") + detail + ").\n\n"
				   "A driver update is the thing to try.");
	}

	printfLog("  Render target:    %dx%d in a %dx%d texture\n",
			  screenSize.x, screenSize.y, frameTextureSize.x, frameTextureSize.y);
}

void Engine::destroyFrameBuffer()
{
	Renderer& renderer = Renderer::inst();
	Renderer::DirectGL direct;
	if(frameDepthStencilID)  { glExtDeleteRenderbuffers(1, &frameDepthStencilID); frameDepthStencilID = 0; }
	if(frameBufferID)        { glExtDeleteFramebuffers(1, &frameBufferID);        frameBufferID = 0; }
	if(frameTextureID)       { renderer.deleteTexture(frameTextureID);            frameTextureID = 0; }
	if(renderTargetID)       { glExtDeleteFramebuffers(1, &renderTargetID);       renderTargetID = 0; }

	for(std::vector<OffscreenTexture>::const_iterator i = offscreenTextures.begin();
		i != offscreenTextures.end(); ++i)
	{
		renderer.deleteTexture(i->id);
	}
	offscreenTextures.clear();
}

uint Engine::acquireOffscreenTexture(const Vec2i& size)
{
	// One of the right size that nobody is holding?
	for(std::vector<OffscreenTexture>::iterator i = offscreenTextures.begin();
		i != offscreenTextures.end(); ++i)
	{
		if(!i->lent && i->size == size) { i->lent = true; return i->id; }
	}

	OffscreenTexture entry;
	// Clamped: WebGL 1 samples a texture whose edges are not a power of two
	// as pure black when it is repeated rather than clamped - with no error.
	// This one is a power of two, but clamped is right here anyway.
	entry.id = Texture::createGLTexture(size, 0, true, true, true);
	entry.size = size;
	entry.lent = true;
	if(!entry.id) return 0;

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
	if(!textureID) return false;

	// The bracket flushes, which is what makes a bake inside an open batch
	// safe: quads queued before it belong on the screen and go up before the
	// target moves, quads queued during it belong on the texture.
	Renderer::DirectGL direct;

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

	// The projection, the transform and the scissor are the renderer's:
	// its target begins from the texture's origin whichever transform the
	// caller stood under, with no scissor of the frame's clipping it.
	glViewport(0, 0, size.x, size.y);
	Renderer::inst().beginTarget(size);
	return true;
}

void Engine::endRenderToTexture()
{
	// The bracket flushes the bake onto the texture while it is still the
	// target; then the target goes back to the frame.
	Renderer::DirectGL direct;
	Renderer::inst().endTarget();

	// Detach the texture again: it is read in a moment, and a target that
	// doubles as a source is undefined.
	glExtFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
							  GL_TEXTURE_2D, 0, 0);
	bindFrameBuffer();
}

void Engine::bindFrameBuffer()
{
	Renderer::DirectGL direct;
	glExtBindFramebuffer(GL_FRAMEBUFFER_EXT, frameBufferID);
	glViewport(0, 0, screenSize.x, screenSize.y);
}

void Engine::unbindFrameBuffer()
{
	Renderer::DirectGL direct;
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

bool Engine::isPadShown() const
{
	return EM_ASM_INT({ return (window.b5pad && window.b5pad.isVisible()) ? 1 : 0; }) != 0;
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
	// The window has to be the windowed one. In fullscreen it is the
	// screen-sized popup with nothing here to read, which is why
	// setFullScreen() asks before it switches rather than after.
	if(fullScreen) return;

	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if(!SDL_GetWMInfo(&info) || !info.window) return;

	// GetWindowPlacement rather than GetWindowRect: for a maximized window
	// GetWindowRect gives the maximized frame, whose corner sits off the
	// screen by the width of the invisible grab handles. rcNormalPosition is
	// what "restore" goes back to, and that is what gets saved.
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

	// Both rectangles, because they are not in the same coordinate system and
	// the difference between them is the whole of the trap this function and
	// restoreWindowPosition() are written around: rcNormalPosition is in
	// *workspace* coordinates - the work area, with the taskbar and any docked
	// toolbar taken out of it - where GetWindowRect gives *screen* ones. The
	// two agree exactly while the work area begins at the top left corner of
	// the monitor, which is what a taskbar along the bottom or the right
	// gives, and that is why the difference is invisible on almost every
	// machine. Written down rather than reasoned about: nothing that builds
	// this tree can run it.
	RECT onScreen = { 0, 0, 0, 0 };
	GetWindowRect(info.window, &onScreen);
	printfLog("  Window: normal %d,%d %dx%d, on screen %d,%d, maximized %d\n",
			  static_cast<int>(wp.rcNormalPosition.left),
			  static_cast<int>(wp.rcNormalPosition.top),
			  static_cast<int>(wp.rcNormalPosition.right  - wp.rcNormalPosition.left),
			  static_cast<int>(wp.rcNormalPosition.bottom - wp.rcNormalPosition.top),
			  static_cast<int>(onScreen.left), static_cast<int>(onScreen.top),
			  maximized ? 1 : 0);
#endif
}

void Engine::restoreWindowPosition()
{
#ifdef _WIN32
	if(!windowedPositionKnown) return;

	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if(!SDL_GetWMInfo(&info) || !info.window) return;

	// SetWindowPlacement and not SetWindowPos, because what was saved is
	// rcNormalPosition and that is in workspace coordinates while SetWindowPos
	// takes screen ones. The two only round-trip while the work area starts at
	// the top left corner of the monitor; where it does not - a taskbar along
	// the top or the left - every save and restore shifts the window by the
	// size of it, in the same direction each time, and over a run of sessions
	// the window walks across the desktop.
	//
	// It carries two other things that used to be done by hand here: showCmd
	// is the whole of the maximized state, and a placement that would put the
	// window on no screen at all is moved back onto one by Windows itself.
	WINDOWPLACEMENT wp;
	wp.length = sizeof(wp);
	if(!GetWindowPlacement(info.window, &wp)) return;

	// rcNormalPosition is a window rect and windowedSize a client area, so the
	// frame has to be added back: AdjustWindowRectEx on an empty rectangle is
	// exactly what the border costs, and this is rememberWindowPlacement()'s
	// own arithmetic run backwards. Where it fails the rectangle keeps the size
	// it already had.
	LONG width  = wp.rcNormalPosition.right  - wp.rcNormalPosition.left;
	LONG height = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;
	RECT frame = { 0, 0, 0, 0 };
	const LONG style   = GetWindowLong(info.window, GWL_STYLE);
	const LONG exStyle = GetWindowLong(info.window, GWL_EXSTYLE);
	if(AdjustWindowRectEx(&frame, style & ~WS_MAXIMIZE, FALSE, exStyle))
	{
		width  = windowedSize.x + (frame.right  - frame.left);
		height = windowedSize.y + (frame.bottom - frame.top);
	}

	wp.rcNormalPosition.left   = windowedPosition.x;
	wp.rcNormalPosition.top    = windowedPosition.y;
	wp.rcNormalPosition.right  = windowedPosition.x + width;
	wp.rcNormalPosition.bottom = windowedPosition.y + height;

	// The maximized state is part of the placement rather than a separate
	// ShowWindow(), and replaying it costs the caller nothing: DIB_ResizeWindow
	// does its whole body inside if(!SDL_windowid && !IsZoomed(SDL_Window)), so
	// the SDL_SetVideoMode that follows moves no window while this one is
	// maximized - it resizes SDL's own surface and the viewport and stops.
	wp.showCmd = maximized ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL;
	SetWindowPlacement(info.window, &wp);
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
	if(!inSizeMove || !initialized) return;

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

void Engine::applyWindowStyle(bool wantFullScreen, const Vec2i& size)
{
	// What handleResize() is finally told. Leaving fullscreen into a maximized
	// window is the one case where it is not what the caller asked for.
	Vec2i clientSize = size;

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
			//
			// The style is all that is kept here. Where the window stood and
			// whether it was maximized belongs to setFullScreen(), which asks
			// rememberWindowPlacement() before it switches: GetWindowRect on a
			// maximized window gives the maximized frame, whose corner hangs
			// off the screen, and it answers in screen coordinates where the
			// rest of this pair works in the workspace ones.
			if(!savedWindowStyle) savedWindowStyle = static_cast<long>(GetWindowLong(hwnd, GWL_STYLE));

			SetWindowLong(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
			// HWND_TOP, not HWND_TOPMOST: a borderless fullscreen window that
			// sticks above everything makes Alt+Tab useless.
			SetWindowPos(hwnd, HWND_TOP, 0, 0, size.x, size.y,
						 SWP_FRAMECHANGED | SWP_SHOWWINDOW);
		}
		else if(savedWindowStyle && windowedPositionKnown)
		{
			// The style first, because restoreWindowPosition() computes the
			// frame from the style that is set when it runs. It then puts the
			// position, the size and the maximized state back through the same
			// API that took them, which is what keeps the workspace
			// coordinates of rcNormalPosition round-tripping.
			SetWindowLong(hwnd, GWL_STYLE, savedWindowStyle);
			restoreWindowPosition();
			savedWindowStyle = 0;

			// The size the window actually became, and not the one the caller
			// offered: setFullScreen() has only the *windowed* size to hand
			// over, and a window that has just come back maximized is the size
			// of the work area instead. Passing the windowed size on would
			// resize the maximize away in the same breath as restoring it.
			RECT client;
			if(GetClientRect(hwnd, &client) && client.right > 0 && client.bottom > 0)
			{
				clientSize = Vec2i(static_cast<int>(client.right),
								   static_cast<int>(client.bottom));
			}
		}
		else
		{
			// No remembered position - back into a window all the same. There
			// must always be a way out of fullscreen. This rectangle is
			// computed against the desktop rather than read off a window, so it
			// is in screen coordinates and SetWindowPos is what takes those.
			long style = savedWindowStyle;
			if(!style) style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
			RECT r = { 0, 0, size.x, size.y };
			AdjustWindowRect(&r, style, FALSE);
			const int w = r.right - r.left;
			const int h = r.bottom - r.top;
			const Vec2i desktop = getDesktopSize();
			int x = (desktop.x - w) / 2;
			int y = (desktop.y - h) / 2;
			if(x < 0) x = 0;
			if(y < 0) y = 0;

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
	handleResize(clientSize.x, clientSize.y);
}

bool Engine::isFullScreen() const
{
#ifdef __EMSCRIPTEN__
	return EM_ASM_INT({ return (document.fullscreenElement ||
								document.webkitFullscreenElement) ? 1 : 0; }) != 0;
#else
	return fullScreen;
#endif
}

void Engine::setFullScreen(bool wantFullScreen)
{
#ifdef __EMSCRIPTEN__
	fullScreen = isFullScreen();
#endif
	if(!initialized || fullScreen == wantFullScreen) { fullScreen = wantFullScreen; return; }

	// Going fullscreen takes the windowed placement away - the window becomes
	// the screen-sized popup - so Engine::exit() would find nothing left to
	// read. It is taken here instead, while the window is still the one
	// config.xml is about.
	if(wantFullScreen) rememberWindowPlacement();

	fullScreen = wantFullScreen;
	printfLog("* %s\n", wantFullScreen ? "Going fullscreen" : "Leaving fullscreen");

#ifdef __EMSCRIPTEN__
	// In the browser the Fullscreen API does this, and it demands a transient
	// user activation - hence only from the Alt+Return callback at the DOM.
	emscriptenSetFullScreen(wantFullScreen);
#else
	applyWindowStyle(wantFullScreen, wantFullScreen ? getDesktopSize() : windowedSize);
#endif
}

void Engine::handleResize(int width, int height)
{
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
	if(!fullScreen && !isWindowMaximized()) windowedSize = displaySize;
}

Vec2f Engine::warpToSource(const Vec2f& p) const
{
	return p_wantedUpscaler->warpToSource(p);
}

Vec2f Engine::warpToOutput(const Vec2f& p) const
{
	return p_wantedUpscaler->warpToOutput(p);
}

void Engine::computePresentRect(int& x, int& y, int& w, int& h) const
{
	// The largest possible 4:3 rectangle in the window, centred. What is left
	// over goes black - black bars rather than a distorted picture.
	float scale = min(static_cast<float>(displaySize.x) / screenSize.x,
					  static_cast<float>(displaySize.y) / screenSize.y);

	// Sharp needs an integer step. At a fractional factor nearest doubles some
	// source pixels and not others - uneven stroke widths, ragged lettering.
	// Below 1:1 there is no such step.
	if(p_wantedUpscaler->wantsIntegerScale() && scale >= 1.0f) scale = floorf(scale);

	w = static_cast<int>(screenSize.x * scale);
	h = static_cast<int>(screenSize.y * scale);
	x = (displaySize.x - w) / 2;
	y = (displaySize.y - h) / 2;
}

void Engine::presentFrame()
{
	int x, y, w, h;
	computePresentRect(x, y, w, h);

	// Raw, inside a bracket: whatever the overlays queued goes up first, and
	// the renderer forgets what GL holds at the end and applies all of it
	// again at its next flush - the blend enable, the tests, the mask - so
	// nothing switched here has to be put back. The mask is set rather than
	// assumed: a scope that closed after the frame's last draw leaves its
	// mask in GL until that next flush, which comes after this.
	Renderer::DirectGL direct;
	glDisable(GL_BLEND);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_SCISSOR_TEST);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
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
	context.vertexBuffer = presentVertexBuffer;

	Upscaler* p_upscaler = p_wantedUpscaler;

	// Sharp and Smooth are nothing but this setting; SharpFit and the CRT
	// filter remap the texture coordinate for the hardware interpolation to
	// give the wanted result, and therefore need GL_LINEAR as well.
	const GLint filter = p_upscaler->getTextureFilter();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

	p_upscaler->present(context);

	glBindTexture(GL_TEXTURE_2D, 0);
}

void Engine::drawOverlays()
{
	if(p_muteIconTexture && getEffectiveSoundVolume() == 0.0f && getEffectiveMusicVolume() == 0.0f)
	{
		renderSprite(p_muteIconTexture, Vec2i(5, 5),
					 muteIconPositionOnTexture, muteIconSize, Vec4f(1.0f, 1.0f, 1.0f, 0.75f));
	}

	if(p_recordingIconTexture && p_videoRecorder &&
	   ((getExactTimeMS() - recordingStartTime) / 500) % 2)
	{
		renderSprite(p_recordingIconTexture, Vec2i(screenSize.x - recordingIconSize.x - 5, 5),
					 recordingIconPositionOnTexture, recordingIconSize, Vec4f(1.0f, 1.0f, 1.0f, 0.75f));
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
	// The small font, and everything on one line: this stands over the game
	// while the game is what is being measured, and a block of three lines in
	// the GUI font covers a tenth of the picture.
	Font* p_font = GUI::inst().getToolTipFont();
	if(!p_font) return;

	// The frame rate off the interval and the rest off the work: the two
	// differ whenever something else sets the pace, which in the browser
	// requestAnimationFrame always does.
	const float interval = frameStats.getPercentile(FrameStats::FS_INTERVAL, 50);
	// The budget is the logic rate - 20 ms, fifty frames a second - and the
	// two counts answer different questions. A frame whose *interval* went
	// over is one the player did not get; one whose *work* went over is one
	// this game is responsible for. They come apart exactly where it matters:
	// under swiftshader the browser ran at 38 ms a frame on 2.7 ms of work,
	// so counting the work alone would have reported nothing wrong while the
	// game ran at 26 fps.
	//
	// **The interval is counted against two ticks and the work against one**,
	// and the asymmetry is the whole point. The loop aims each iteration at
	// exactly one tick - the SDL_Delay at the foot of mainLoopIteration - so
	// an interval threshold of one tick sits on the number the code is
	// targeting, and a millisecond of timer granularity trips it: measured in
	// the menu, 277 of 512 frames read as late while not one had been dropped.
	// A frame the player actually lost is an interval of two. The work has no
	// such problem, because nothing aims it anywhere: one tick is simply the
	// budget a frame has to fit inside.
	//
	// 500 ms is a third question. That is what Emscripten's OpenAL has
	// scheduled ahead (AL.QUEUE_LOOKAHEAD, raised in initOpenAL), so a frame
	// longer than that is a hole in the music - in the browser only, since
	// natively the decoder thread fills the queue whatever the main thread is
	// doing.
	//
	// One line, and one grammar for every token in it: name, colon, value. A
	// token that *ends* in a colon is a heading instead, and what follows it
	// is read against it until the next one - which is how the two triples say
	// once, rather than twice, that they are p50, p95 and the maximum. In
	// brackets is the ring's fill, the window all of it is over: 500 once ten
	// seconds have run, less while it fills.
	//
	// The colon is what lets one space separate the tokens: it binds a name to
	// its value more tightly than any amount of space, so nothing has to be
	// grouped by a wider gap and nothing is glued together to save one. It is
	// also narrower than the space it replaces - 3 px against 5 - which is why
	// the four phases can afford a name each here where they shared one
	// before.
	//
	// The phases are medians, in the same milliseconds as the triple before
	// them: a percentile of a part would not add up to one of the whole. The
	// three counts are what the player would have noticed - a frame they did
	// not get, one the game did not fit into its budget, one long enough to
	// leave a hole in the music - and perf.md has the thresholds, which are
	// constants and do not need restating fifty times a second.
	//
	// It is written to fit at its widest, not at its usual, because the
	// numbers grow exactly when something is wrong. Measured against the
	// font's own advances: 531 px of 640 as it usually reads, 548 with a level
	// load's stall still in the window, and 633 under -flushall on a slow
	// machine, where every quad is its own draw and the milliseconds, the
	// draws and the counts stand at their widest at once. That last arm is the
	// one the spaces used to cost: the same line with a space for every colon
	// and wider gaps between the groups measured 665, and lost its tail.
	const float budget = static_cast<float>(logicRate);
	char line[160];
	snprintf(line, sizeof(line),
			 "fps:%.0f 50/95/max(%u): ms:%.1f/%.1f/%.1f draws:%.0f/%.0f/%.0f"
			 " r:%.1f u:%.1f p:%.1f s:%.1f late:%u slow:%u stall:%u",
			 interval > 0.0f ? 1000.0f / interval : 0.0f,
			 frameStats.getCount(),
			 frameStats.getPercentile(FrameStats::FS_TOTAL, 50),
			 frameStats.getPercentile(FrameStats::FS_TOTAL, 95),
			 frameStats.getPercentile(FrameStats::FS_TOTAL, 100),
			 frameStats.getPercentile(FrameStats::FS_DRAWS, 50),
			 frameStats.getPercentile(FrameStats::FS_DRAWS, 95),
			 frameStats.getPercentile(FrameStats::FS_DRAWS, 100),
			 frameStats.getPercentile(FrameStats::FS_RENDER, 50),
			 frameStats.getPercentile(FrameStats::FS_UPDATE, 50),
			 frameStats.getPercentile(FrameStats::FS_PRESENT, 50),
			 frameStats.getPercentile(FrameStats::FS_SWAP, 50),
			 frameStats.getCountOver(FrameStats::FS_INTERVAL, 2.0f * budget),
			 frameStats.getCountOver(FrameStats::FS_TOTAL, budget),
			 frameStats.getCountOver(FrameStats::FS_TOTAL, 500.0f));

	// The strip is as wide as the line and no wider, so that what it covers is
	// only what it has to.
	Vec2i dimensions;
	p_font->measureText(line, &dimensions, 0);
	const int height = p_font->getLineHeight() + 4;
	const int top = screenSize.y - height;

	Renderer::inst().setBlend(BM_NORMAL);
	Renderer::inst().rect(Vec2f(0.0f, static_cast<float>(top)),
						  Vec2f(static_cast<float>(dimensions.x + 8), static_cast<float>(screenSize.y)),
						  Vec4f(0.0f, 0.0f, 0.0f, 0.7f));

	p_font->renderText(line, Vec2i(4, top + 2), Vec4f(1.0f, 1.0f, 1.0f, 1.0f));
}

bool Engine::encodeFrame(std::vector<uchar>* p_pngOut)
{
	// Always the internal 640x480 frame: the filter and the black bars are
	// display settings and do not belong in the file.
	const Vec2i shotSize(screenSize);

	// GL_RGBA and not GL_RGB or GL_BGR: that is the only combination WebGL 1
	// allows too. Only the three colour channels of it reach the file - see
	// img_save.h, the alpha would be a quarter more for nothing but 255. The
	// encoder flips the rows along the way; no second buffer is needed.
	std::vector<uchar> pixels(static_cast<size_t>(shotSize.x) * shotSize.y * 4);
	// The frame belongs to the game's own framebuffer, so this binds it rather
	// than reading GL_COLOR_ATTACHMENT0 of whatever happens to be bound. The
	// two callers inside the main loop have it bound already - the screenshot
	// key and the video recorder both sit in the frameRendered block, above
	// the unbindFrameBuffer() that precedes the present - but a caller from
	// anywhere else does not, and the attachment it would read then is not
	// this game's picture. It also puts the viewport back to 640x480, which
	// is the size this read assumes.
	bindFrameBuffer();
	readFrame(&pixels[0]);

	if(!encodePNG(&pixels[0], shotSize, 4, 3, true, p_pngOut))
	{
		printfLog("+ ERROR: Could not encode the screenshot.\n");
		return false;
	}

	return true;
}

bool Engine::writeScreenshot(const std::string& path)
{
	// The frame oracle's half of screenshot(): a name the caller chose instead
	// of the dated one, and no sound. It goes through FileSystem like every
	// other write, so the browser's IDBFS path works too if a test ever wants
	// it.
	std::vector<uchar> png;
	if(!encodeFrame(&png)) return false;

	FileSystem& fs = FileSystem::inst();
	File* p_file = fs.openFile(path, FileSystem::FM_WRITE);
	if(!p_file)
	{
		printfLog("+ ERROR: Could not write \"%s\".\n", path.c_str());
		return false;
	}

	const uint numBytes = static_cast<uint>(png.size());
	const bool saved = p_file->write(&png[0], numBytes) == numBytes && p_file->finish();
	fs.closeFile(p_file);
	if(!saved) printfLog("+ ERROR: Could not write \"%s\".\n", path.c_str());
	return saved;
}

bool Engine::screenshot()
{
	std::vector<uchar> png;
	if(!encodeFrame(&png)) return false;

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

void Engine::renderSprite(const Vec2f& position,
						  const Vec2i& positionOnTexture,
						  const Vec2i& size,
						  const Vec4f& color,
						  bool mirrorX,
						  float rotation,
						  float scaling)
{
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

	Renderer::inst().sprite(position, halfSize, otherHalf, u0, u1, v0, v1, color, rotation, scaling);
}

void Engine::enableFlushAll()
{
	Renderer::inst().setFlushAll(true);
}

void Engine::renderSprite(Texture* p_sprite,
						  const Vec2f& position,
						  const Vec2i& positionOnTexture,
						  const Vec2i& size,
						  const Vec4f& color,
						  bool mirrorX,
						  float rotation,
						  float scaling)
{
	Renderer::inst().setTexture(p_sprite->ref());
	renderSprite(position, positionOnTexture, size, color, mirrorX, rotation, scaling);
}

void Engine::renderSprites(const Sprites& sprites,
						   const Vec4f& color)
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
								 float pitchSpectrum,
								 int priority,
								 bool forceCreation)
{
	if(!filename.length()) return 0;

	Sound* p_sound = Manager<Sound>::inst().request(filename);
	if(p_sound)
	{
		// The pitch is drawn whether or not the instance comes.
		// Sound::createInstance drops a one-shot that follows the same sound
		// within ten milliseconds of wall time, and a draw behind that made
		// the generator's sequence - which every tick of a level runs
		// through in order - depend on how the machine bunched its ticks:
		// the same level came out differently from one run to the next.
		const float pitch = pitchSpectrum != 0.0f ? 1.0f + random(-pitchSpectrum, pitchSpectrum) : 1.0f;

		SoundInstance* p_inst = p_sound->createInstance(forceCreation);
		p_sound->release();

		if(p_inst)
		{
			// set the pitch
			if(pitchSpectrum != 0.0f) p_inst->setPitch(pitch);

			// set the priority
			p_inst->setPriority(priority);

			// play
			p_inst->play(loop);
		}

		return p_inst;
	}

	return 0;
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
					   float loopBegin,
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

				p_currentMusic->setVolume(0.0f);
				p_currentMusic->play(loopBegin != -1.0f);
				p_currentMusic->slideVolume(1.0f, 0.02f);
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

		p_currentMusic->slideVolume(-1.0f, 0.02f);
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
	p_action->tertiary = -1;
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

// One cell along `axis` (0 for x, 1 for y) toward the cursor, and nothing
// where that axis has already arrived.
static Vec2i dragStep(int axis,
					  const Vec2i& away)
{
	Vec2i step(0, 0);
	if(axis == 0)
	{
		if(away.x) step.x = (away.x < 0) ? -1 : 1;
	}
	else if(away.y) step.y = (away.y < 0) ? -1 : 1;
	return step;
}

// The drag names a cell rather than a direction: the character walks to
// whatever the cursor is over and keeps going while a button is held. That
// costs nothing in here - the four direction keys are held exactly as a
// finger holds an arrow key, so the step cadence, the opposing-action rule
// and the rest of the action layer apply unchanged.
//
// Two rules are worth the words. The keys are *held* and never pulsed: a
// press that lands while an action's repeat is counting down goes into its
// buffer (updateActions) and is played out later, so a pulsed key would stack
// steps up and walk on after the player let go. And only one axis moves at a
// time, committed until it runs out - stepping whichever axis is further off
// each tick draws a staircase, and while that is no faster than the two
// straight legs it is a path nobody would walk by hand on the keyboard, so it
// would be an advantage for nothing.
//
// Two more belong to the game rather than to the gesture, and are asked of
// the game state: whether there is anybody to steer, which includes the press
// having landed on a character, and whether the way a leg wants to go is open
// at all (getMouseDragCells, canMouseDragStep).
void Engine::updateMouseDrag()
{
	const bool leftButton = isButtonDown(SDL_BUTTON_LEFT);
	const bool rightButton = isButtonDown(SDL_BUTTON_RIGHT);
	const int buttons = (leftButton ? 1 : 0) | (rightButton ? 2 : 0);

	if(!buttons)
	{
		// Every button up ends the drag and clears a block, so that the next
		// press is free to start a new one.
		dragButtons = 0;
		dragAxis = -1;
		dragBlocked = false;
	}

	Vec2i actor, target;
	GameState* p_gs = getGameState();
	const bool steering = buttons && !dragBlocked
						  && p_gs && p_gs->getMouseDragCells(&actor, &target);

	Vec2i step(0, 0);
	if(!steering) dragAxis = -1;
	else
	{
		const Vec2i away = target - actor;
		if(away.isZero()) dragAxis = -1;
		else
		{
			// The buttons are read again every tick until the character has
			// somewhere to go, because somebody reaching for both of them
			// presses one a moment before the other and the pair is what they
			// meant.
			if(!dragButtons) dragButtons = buttons;

			// A leg begins where there is no axis yet or the one being walked
			// has run out, and it takes whichever is further off.
			int axis = dragAxis;
			if(axis == -1
			   || (axis == 0 && away.x == 0)
			   || (axis == 1 && away.y == 0))
			{
				axis = (abs(away.x) >= abs(away.y)) ? 0 : 1;
			}

			step = dragStep(axis, away);
			if(!p_gs->canMouseDragStep(step))
			{
				// A leg that has walked into something is over as surely as
				// one that has run out, and the other axis is what takes the
				// character around the obstacle. Where that is blocked too,
				// or has arrived already, the drag commands nothing at all
				// rather than leaning on the wall for as long as the button
				// is held.
				axis = 1 - axis;
				step = dragStep(axis, away);
				if(!p_gs->canMouseDragStep(step)) step = Vec2i(0, 0);
			}

			dragAxis = step.isZero() ? -1 : axis;
		}
	}

	virtualKeys[getMouseDragVK(MOUSE_DRAG_LEFT)].down = step.x < 0;
	virtualKeys[getMouseDragVK(MOUSE_DRAG_RIGHT)].down = step.x > 0;
	virtualKeys[getMouseDragVK(MOUSE_DRAG_UP)].down = step.y < 0;
	virtualKeys[getMouseDragVK(MOUSE_DRAG_DOWN)].down = step.y > 0;

	// What the drag carries was settled when it set off and does not change
	// while it runs. Reading it live would be a trap: on the way into a
	// two-button grip there is a tick with only the right button down, and
	// that is the gesture for a *lit* bomb - the player would get one where
	// they asked for a bomb put down safely.
	const bool carrying = !step.isZero();
	virtualKeys[getMouseDragVK(MOUSE_DRAG_PLANT)].down = carrying && (dragButtons == 2);
	virtualKeys[getMouseDragVK(MOUSE_DRAG_PUT_DOWN)].down = carrying && (dragButtons == 3);
}

int Engine::getMouseDragVK(int which) const
{
	if(which < 0 || which >= NUM_MOUSE_DRAG_VKS) return -1;
	// Not read off virtualKeys: main.cpp asks before Engine::init has built
	// it. The six are pushed directly behind the keyboard block, so the base
	// is the length of that block.
	return static_cast<int>(SDLK_LAST) + which;
}

void Engine::cancelMouseDrag()
{
	dragButtons = 0;
	dragAxis = -1;
	// Blocked, not merely ended: the buttons are still held, and without this
	// the next movement would begin a fresh drag under the open menu.
	dragBlocked = true;
	for(int i = 0; i < NUM_MOUSE_DRAG_VKS; i++)
	{
		virtualKeys[getMouseDragVK(i)].down = false;
	}
}

void Engine::updateVKs()
{
	updateMouseDrag();

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
		if(vk.device == VK_DEVICE_MOUSE)
		{
			// set by updateMouseDrag() above, not polled
		}
		else if(vk.device == -1)
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
		if(a.tertiary != -1) down |= virtualKeys[a.tertiary].down;

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

	// Exactly the inverse of what presentFrame() draws. The rectangle is centred,
	// so the arithmetic holds in SDL's window coordinates as well as in GL's.
	// Computed with pixel centres; only that makes the round trip exact.
	int x, y, w, h;
	computePresentRect(x, y, w, h);
	if(w > 0 && h > 0)
	{
		Vec2f n((position.x + 0.5f - x) / w, (position.y + 0.5f - y) / h);

		// The same curvature as in the shader: the cursor sits on the glass.
		// Without curvature warpToSource returns the coordinate unchanged.
		const Vec2f warped = warpToSource(n * 2.0f - Vec2f(1.0f, 1.0f));
		n = (warped + Vec2f(1.0f, 1.0f)) * 0.5f;

		position.x = static_cast<int>(floorf(n.x * screenSize.x));
		position.y = static_cast<int>(floorf(n.y * screenSize.y));
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

	int x, y, w, h;
	computePresentRect(x, y, w, h);
	if(w > 0 && h > 0)
	{
		Vec2f n((temp.x + 0.5f) / screenSize.x, (temp.y + 0.5f) / screenSize.y);

		// The way back through the curvature. With the CRT filter off this
		// is the identity.
		const Vec2f out = warpToOutput(n * 2.0f - Vec2f(1.0f, 1.0f));
		n = (out + Vec2f(1.0f, 1.0f)) * 0.5f;

		temp.x = x + static_cast<int>(floorf(n.x * w));
		temp.y = y + static_cast<int>(floorf(n.y * h));
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

uint Engine::getTime() const
{
	return time;
}

const Vec2i& Engine::getScreenSize() const
{
	return screenSize;
}

uint Engine::createFrameCopyTexture(bool withAlpha,
									bool smooth)
{
	// Repeating, not clamped: getFrameCopyRef's scale reaches the band
	// through the wrap.
	return Texture::createGLTexture(screenPow2Size, 0, withAlpha, smooth, false);
}

void Engine::captureFrame(uint textureID)
{
	Renderer::inst().copyFrame(textureID, Vec2i(0, screenPow2Size.y - screenSize.y), screenSize);
}

TextureRef Engine::getFrameCopyRef(uint textureID) const
{
	// What a screen-sized copy of the frame is sampled with, for texture
	// coordinates given in the game's own pixels. The y is negative for two
	// reasons at once: the game's y runs downward where GL's texture y runs
	// up, and the copy sits at the top of the pow2 texture rather than at its
	// origin - under GL_REPEAT a negative coordinate wraps to exactly that
	// band. That wrap is the reason the ref says it tiles: it is a render
	// target read back rather than a picture from a file, so no atlas could
	// hold it anyway, but the renderer's check has to be told so.
	return TextureRef(textureID,
					  Vec2f(1.0f / static_cast<float>(screenPow2Size.x), -1.0f / static_cast<float>(screenPow2Size.y)),
					  Vec2f(0.0f, 0.0f), true);
}

void Engine::readFrame(uchar* p_rgba)
{
	// The framebuffer object is bound, and its read buffer is its one colour
	// attachment from the moment it exists; WebGL has no glReadBuffer to say
	// so with.
	Renderer::DirectGL direct;
	glReadPixels(0, 0, screenSize.x, screenSize.y, GL_RGBA, GL_UNSIGNED_BYTE, p_rgba);
}

const Vec2i& Engine::getDisplaySize() const
{
	return displaySize;
}

// Milliseconds into a running transition, negative through its lead-in and -1
// where there is none - the same number checkFreeze() is handed. Nothing in
// the game asks; the test hook reports it, which is what makes the length of a
// transition something a harness can measure rather than something somebody
// counts under their breath while pressing a key.
int Engine::getCrossfadeProgressMs() const
{
	return p_crossfade ? static_cast<int>(crossfadeTime * 1000.0f) : -1;
}

void Engine::crossfade(Crossfade* p_crossfade,
					   float duration,
					   bool immediately)
{
	if(!p_crossfade || duration <= 0.0f)
	{
		// cancel the crossfade
		delete this->p_crossfade;
		this->p_crossfade = 0;
		crossfadeTime = -1.0f;
		crossfadeDuration = 0.0f;
	}
	else
	{
		// start the crossfade - and let go of one still running, which would
		// otherwise be leaked with whatever textures it holds
		delete this->p_crossfade;
		this->p_crossfade = p_crossfade;
		crossfadeTime = -0.51f;
		crossfadeDuration = duration;
	}

	if(immediately)
	{
		// save the old image - as above out of the framebuffer object, not out
		// of whatever is bound right now.
		bindFrameBuffer();
		captureFrame(oldImageID);
		crossfadeTime = -0.5f;
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
	// Only "de" or "en". Of the 440 strings in data/languages.txt exactly one
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
	soundVolume = musicVolume = 1.0f;
	particleDensity = 1.0f;
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

		// Read the upscaling filter.
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
		// Only before the window exists: read while the game runs - the options
		// dialog's Cancel reloads the file - it would overwrite what handleResize()
		// and applyWindowStyle() know about the window that is actually up.
		TiXmlElement* p_window = p_config->FirstChildElement("Window");
		if(p_window && !initialized)
		{
			// Negative values are allowed: a second screen to the left of the
			// first has them, and so does a window on a monitor above the
			// primary one. A spot that no longer exists at all is Windows'
			// problem - SetWindowPlacement puts such a window back on a screen.
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
			if(p_text) setSoundVolume(static_cast<float>(atof(p_text)));
		}

		// read the music volume
		TiXmlElement* p_musicVolume = p_config->FirstChildElement("MusicVolume");
		if(p_musicVolume)
		{
			const char* p_text = p_musicVolume->GetText();
			if(p_text) setMusicVolume(static_cast<float>(atof(p_text)));
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

float Engine::getSoundVolume() const
{
	return soundVolume;
}

void Engine::setSoundVolume(float soundVolume)
{
	soundVolume = clamp(soundVolume, 0.0f, 1.0f);

	this->soundVolume = soundVolume;
	volumeChanged = true;
}

float Engine::getMusicVolume() const
{
	return musicVolume;
}

void Engine::setMusicVolume(float musicVolume)
{
	musicVolume = clamp(musicVolume, 0.0f, 1.0f);

	this->musicVolume = musicVolume;
	volumeChanged = true;
}

float Engine::getEffectiveSoundVolume() const
{
	// The mute key and a lost focus silence the output here rather than by
	// writing 0 into the setting: the options dialog and config.xml keep
	// seeing the volume the user chose, and neither can overwrite the
	// other's idea of it.
	return muted || !appActive ? 0.0f : soundVolume;
}

float Engine::getEffectiveMusicVolume() const
{
	return muted || !appActive ? 0.0f : musicVolume;
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

	if(details == 0) setParticleDensity(0.333f);
	else if(details == 1) setParticleDensity(0.666f);
	else setParticleDensity(1.0f);
}

float Engine::getParticleDensity() const
{
	return particleDensity;
}

void Engine::setParticleDensity(float particleDensity)
{
	particleDensity = clamp(particleDensity, 0.0f, 1.0f);

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
		float volume = 1.0f;
		if(p_file && p_elem->QueryFloatAttribute("volume", &volume) == TIXML_SUCCESS) soundVolumes[p_file] = volume;
	}

	printfLog("* %u sound volume(s) read from \"%s\".\n",
			  static_cast<uint>(soundVolumes.size()), filename.c_str());
}

float Engine::getSoundVolumeFactor(const std::string& filename) const
{
	const std::unordered_map<std::string, float>::const_iterator i =
		soundVolumes.find(filename);
	return (i == soundVolumes.end()) ? 1.0f : i->second;
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

std::string Engine::localizeString(const std::string& text,
								   const std::string& inLanguage)
{
	// The language is swapped for the duration, the way localizeStringRaw()
	// falls back to English inside: so the lookup, the section picked and
	// the key names the bindings expand to all follow the one asked for.
	const std::string active = language;
	language = inLanguage;
	const std::string result = localizeString(text);
	language = active;
	return result;
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
	const float scale = screenSize.x ? static_cast<float>(w) / screenSize.x : 1.0f;

	const int wanted = scale >= 1.5f ? 2 : 1;
	if(wanted == cursorScale) return;

	SDL_Cursor* p_cursor = wanted == 2 ? p_cursor2x : p_cursor1x;
	if(!p_cursor) return;

	cursorScale = wanted;
	SDL_SetCursor(p_cursor);
}
