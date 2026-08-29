#include "pch.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
// Definition weiter unten, bei setFullScreen().
static EM_BOOL engineFullScreenHotkey(int, const EmscriptenKeyboardEvent*, void*);
#endif
#ifdef _WIN32
// Fuer den Vollbildwechsel: der Fensterstil wird direkt gesetzt, an SDL vorbei.
#include <SDL_syswm.h>
#endif
#include "engine.h"
#include "glextensions.h"
#include "xbr_lv2.h"
#include "sharpfit_shader.h"
#include "gamestate.h"
#include "soundinstance.h"
#include "texture.h"
#include "font.h"
#include "sound.h"
#include "streamedsound.h"
#include "gui.h"
#include "tileset.h"
#include "crossfade.h"
#include "filesystem.h"
#include "videorecorder.h"
#include "audiocapture.h"

Engine::Engine()
{
	initialized = false;

	for(int i = 0; i < NUM_KEY_SLOTS; i++)
	{
		keyData[i] = 0;
		buttonData[i] = 0;
	}

	frameTime = 0;
	time = 0;
	modal = false;
	p_crossfade = 0;
	crossfadeTime = -1.0;
	crossfadeDuration = 0.0;
	glExtBlendFuncSeparate = 0;
	p_videoRecorder = 0;
	p_audioCapture = 0;
	p_muteIconTexture = 0;
	frameBufferID = 0;
	frameTextureID = 0;
	frameDepthStencilID = 0;
	useFrameBuffer = false;
	upscaleFilter = UF_XBR_DETAIL;   // Voreinstellung; ohne Shader wird bilinear daraus
	fullScreen = false;
	fullScreenOverride = -1;
	swallowedReturn = false;
	windowedSize = Vec2i(0, 0);      // init() setzt das auf screenSize
	savedWindowStyle = 0;
	savedWindowRect[0] = savedWindowRect[1] = savedWindowRect[2] = savedWindowRect[3] = 0;
	xbrProgram = 0;
	xbrDecalLocation = -1;
	xbrTextureSizeLocation = -1;
	xbrSmallDetailsLocation = -1;
	sharpFitProgram = 0;
	sharpFitDecalLocation = -1;
	sharpFitTextureSizeLocation = -1;
	sharpFitFrameSizeLocation = -1;
	sharpFitPrescaleLocation = -1;
	oldSoundVolume = -1.0;
	oldMusicVolume = -1.0;
	timePlayed = 0;
	doScreenshot = false;
}

Engine::~Engine()
{
	exit();
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

	// Reihenfolge: eingebaute Voreinstellung, dann die config.xml, dann die
	// Kommandozeile - die sticht beides. Ab hier zaehlt nur noch fullScreen,
	// nie mehr defaultFullScreen: die beiden koennen auseinanderlaufen, und
	// genau das hat den Vollbildwechsel schon einmal unbrauchbar gemacht.
	fullScreen = defaultFullScreen;
#ifdef __EMSCRIPTEN__
	// Im Browser fuellt die Seite ohnehin schon alles aus, und die
	// Fullscreen-API laesst sich ohne echten Tastendruck gar nicht ausloesen -
	// beim Start gibt es also kein Vollbild, egal was irgendwo steht.
	fullScreen = false;
#endif
	windowedSize = screenSize;

	// Konfiguration laden
	loadConfig();

	if(fullScreenOverride >= 0) fullScreen = (fullScreenOverride != 0);

	printfLog("* Language: %s\n", language.c_str());

	// SDL initialisieren
	printfLog("* Initializing SDL ...\n");
	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_JOYSTICK))
	{
		printfLog("+ ERROR: %s\n", SDL_GetError());
		return false;
	}

	SDL_WM_SetCaption(windowCaption.c_str(), windowCaption.c_str());
	SDL_EnableKeyRepeat(140, 60);
	SDL_EnableUNICODE(1);

	// alle Tasten als VK einfügen
	for(int k = 0; k < SDLK_LAST; k++)
	{
		VirtualKey vk;
		const char* p_name = SDL_GetKeyName(static_cast<SDLKey>(k));
		vk.name = std::string("Keyboard ") + (p_name ? p_name : "???");
		vk.key = k;
		vk.down = false;
		virtualKeys.push_back(vk);
	}

	// alle Joysticks öffnen
	int n = SDL_NumJoysticks();
	int index = 0;
	for(int j = 0; j < n; j++)
	{
		SDL_Joystick* p_joystick = SDL_JoystickOpen(j);
		if(p_joystick)
		{
			// alle Tasten als VK einfügen
			int nk = SDL_JoystickNumButtons(p_joystick);
			for(int k = 0; k < nk; k++)
			{
				VirtualKey vk;
				std::ostringstream str;
				str << "Joystick" << index + 1 << " B" << k + 1;
				vk.name = str.str();
				vk.device = index;
				vk.key = k;
				vk.down = false;
				virtualKeys.push_back(vk);
			}

			// alle Achsen als VK einfügen
			int na = SDL_JoystickNumAxes(p_joystick);
			for(int a = 0; a < na; a++)
			{
				VirtualKey vk;

				std::ostringstream str;
				str << "Joystick" << index + 1 << " A" << a + 1 << "-";
				vk.name = str.str();
				vk.device = index;
				vk.axis = a;
				vk.positive = false;
				virtualKeys.push_back(vk);

				str.str("");
				str << "Joystick" << index + 1 << " A" << a + 1 << "+";
				vk.name = str.str();
				vk.device = index;
				vk.axis = a;
				vk.positive = true;
				virtualKeys.push_back(vk);
			}

			// alle Hats mit allen Richtungen als VK einfügen
			int nh = SDL_JoystickNumHats(p_joystick);
			for(int h = 0; h < nh; ++h)
			{
				VirtualKey vk;

				std::ostringstream str;
				str << "Joystick" << index + 1 << " H" << h + 1;
				vk.device = index;
				vk.hat = h;

				vk.name = str.str() + "N";
				vk.hatDir = SDL_HAT_UP;
				virtualKeys.push_back(vk);

				vk.name = str.str() + "NE";
				vk.hatDir = SDL_HAT_RIGHTUP;
				virtualKeys.push_back(vk);

				vk.name = str.str() + "E";
				vk.hatDir = SDL_HAT_RIGHT;
				virtualKeys.push_back(vk);

				vk.name = str.str() + "SE";
				vk.hatDir = SDL_HAT_RIGHTDOWN;
				virtualKeys.push_back(vk);

				vk.name = str.str() + "S";
				vk.hatDir = SDL_HAT_DOWN;
				virtualKeys.push_back(vk);

				vk.name = str.str() + "SW";
				vk.hatDir = SDL_HAT_LEFTDOWN;
				virtualKeys.push_back(vk);

				vk.name = str.str() + "W";
				vk.hatDir = SDL_HAT_LEFT;
				virtualKeys.push_back(vk);

				vk.name = str.str() + "NW";
				vk.hatDir = SDL_HAT_LEFTUP;
				virtualKeys.push_back(vk);
			}

			joysticks.push_back(p_joystick);
			index++;
		}
	}

	limitActionKeys();

	char videoDriver[256] = "";
	SDL_VideoDriverName(videoDriver, 256);
	printfLog("  Video driver: %s\n", videoDriver);

	// OpenGL initialisieren
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
		// Icon laden
		FileSystem& fs = FileSystem::inst();
		File* p_file = fs.openFile(windowIconFilename);
		SDL_RWops* p_rwOps = p_file->getRWOps();
		SDL_Surface* p_surface = IMG_Load_RW(p_rwOps, 1);
		SDL_Surface* p_rgba = SDL_CreateRGBSurface(SDL_SWSURFACE, p_surface->w, p_surface->h, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
		SDL_SetAlpha(p_surface, 0, 0);
		SDL_BlitSurface(p_surface, 0, p_rgba, 0);
		SDL_FreeSurface(p_surface);

		// Maske erzeugen
		SDL_LockSurface(p_rgba);
		Uint8* p_mask = new Uint8[(p_rgba->w + 7) / 8 * p_rgba->h];
		uint cursor = 0;
		for(int y = 0; y < p_rgba->h; y++)
		{
			Uint8 byte = 0;
			for(int x = 0; x < p_rgba->w; x++)
			{
				// Pixelfarbe holen
				uint rgba = reinterpret_cast<uint*>(p_rgba->pixels)[y * (p_rgba->pitch / 4) + x];

				// Alphawert extrahieren
				rgba &= p_rgba->format->Amask;
				rgba >>= p_rgba->format->Ashift;

				// Bit setzen oder nicht setzen
				byte <<= 1;
				if(rgba >= 127) byte |= 1;

				if(!((x + 1) % 8) || x == p_rgba->w - 1)
				{
					// fertiges Byte schreiben
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

	// SDLs Flags bleiben ab hier unveraendert - SDL_OPENGL | SDL_RESIZABLE, das
	// ganze Programm ueber. Nur so trifft DIB_SetVideoMode bei jeder weiteren
	// Groessenaenderung seinen schnellen Pfad, und der GL-Kontext ueberlebt sie.
	// Vollbild ist deshalb kein SDL-Flag, sondern ein Fensterstil (siehe
	// applyWindowStyle); der Aufruf hier legt immer das Fenster an.
	displaySize = windowedSize;
	p_display = SDL_SetVideoMode(displaySize.x, displaySize.y, 32, SDL_OPENGL | SDL_RESIZABLE);
	if(!p_display)
	{
		printfLog("+ ERROR: Could not set video mode (Error: %s).\n", SDL_GetError());
		return false;
	}

	// Startet das Spiel im Vollbild, kommt der Stilwechsel jetzt - das Fenster
	// steht, der Kontext auch.
	if(fullScreen) applyWindowStyle(true, getDesktopSize());

#ifdef __EMSCRIPTEN__
	// Alt+Return am DOM, nicht an SDL: nur ein echter Tastendruck darf die
	// Fullscreen-API ausloesen.
	emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, 0, EM_TRUE,
									engineFullScreenHotkey);
#endif

	SDL_ShowCursor(0);
	setupCursor();

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
	// glBlendFuncSeparate is core in GLES2/WebGL, but the GL_EXT_blend_func_separate
	// extension string is not advertised, so the probe below can never pass. Left
	// alone, glExtBlendFuncSeparate stays null, which costs separate alpha blending
	// and makes Engine::init force GUI opacity to 1.0 - the GUI stops fading.
	glExtBlendFuncSeparate = reinterpret_cast<PFNGLBLENDFUNCSEPARATEEXTPROC>(&glBlendFuncSeparate);
	printfLog("  Separate blending is core in WebGL; using it directly.\n");
	printfLog("  ============================================================\n");
#endif

	// Extensions abfragen
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

	// Bildpuffer anlegen. Schlägt das fehl, rendert das Spiel wie früher direkt
	// in den Backbuffer - dann ist nur die Fenstergröße wieder starr.
	GLExtensions::init();
	useFrameBuffer = createFrameBuffer();
	if(!useFrameBuffer)
	{
		printfLog("- WARNING: No framebuffer object; rendering straight to the back buffer.\n");
	}
	else if(GLExtensions::haveShaders())
	{
		createXbrProgram();
		// Braucht den Vertexpuffer, den createXbrProgram() anlegt.
		if(xbrVertexBuffer) createSharpFitProgram();
	}
	printfLog("  Upscale filters:  nearest, bilinear%s\n",
			  canUseXbr() ? (canUseSharpFit() ? ", sharp-fit, xbr, xbr-details" : ", xbr, xbr-details")
						  : (canUseSharpFit() ? ", sharp-fit" : ""));
	printfLog("  Upscaling:        %s\n", getUpscaleFilterName(getEffectiveUpscaleFilter()));

	// Texturen für Crossfading erzeugen
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

	// OpenAL initialisieren
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

	// Ton für Videoaufnahmen: OpenAL kann nur Eingangsgeräte aufnehmen, also das
	// Mikrofon. Aufgenommen werden soll aber das, was das Spiel ausgibt - das macht
	// AudioCapture über den Loopback-Modus von WASAPI.
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

	printfLog("* Initializing GUI ...\n");
	if(!GUI::inst().init())
	{
		printfLog("+ ERROR: Could not initialize GUI.\n");
		return false;
	}

	// OpenGL-Einstellungen setzen
	glViewport(0, 0, width, height);
#ifndef __EMSCRIPTEN__
	// GL_SMOOTH is the default; emscripten's GL emulation aborts on this call.
	glShadeModel(GL_SMOOTH);
#endif
	glEnable(GL_BLEND);
	glEnable(GL_POINT_SMOOTH);
#ifndef __EMSCRIPTEN__
	// Neither hint target exists in WebGL (both raise INVALID_ENUM).
	glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
#endif

	setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
	if(!glExtBlendFuncSeparate) GUI::inst().setOpacity(1.0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	// Pixel-Bildschirmkoordinaten
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

	if(p_videoRecorder)
	{
		// Aufnahme stoppen
		delete p_videoRecorder;
		p_videoRecorder = 0;
	}

	// aktuellen Spielzustand verlassen
	setGameState("");

	// GUI herunterfahren
	printfLog("* Shutting down GUI ...\n");
	GUI::inst().exit();

	// Bildpuffer freigeben, solange der GL-Kontext noch steht
	destroyXbrProgram();
	destroyFrameBuffer();

	// Manager herunterfahren
	printfLog("* Shutting down resource managers ...\n");
	Manager<TileSet>::inst().exit();
	Manager<Font>::inst().exit();
	Manager<Texture>::inst().exit();
	Manager<Sound>::inst().exit();
	Manager<StreamedSound>::inst().exit();

	// OpenAL herunterfahren
	printfLog("* Shutting down OpenAL ...\n");
	alcSuspendContext(p_audioContext);
	alcMakeContextCurrent(0);
	alcDestroyContext(p_audioContext);
	alcCloseDevice(p_audioDevice);
	delete p_audioCapture;
	p_audioCapture = 0;

	// Crossfade und Texturen löschen
	crossfade(0, 0.0);
	glDeleteTextures(1, &oldImageID);
	glDeleteTextures(1, &newImageID);

	// Joysticks schließen
	for(std::vector<SDL_Joystick*>::const_iterator it = joysticks.begin();
		it != joysticks.end();
		++it)
	{
		SDL_JoystickClose(*it);
	}

	joysticks.clear();

	// SDL herunterfahren
	printfLog("* Shutting down SDL ...\n");
	SDL_Cursor* p_cursor = SDL_GetCursor();
	SDL_FreeCursor(p_cursor);
	SDL_Quit();

	// Aktionen löschen
	for(size_t i = 0; i < actionsVector.size(); i++) delete actionsVector[i];
	actionsVector.clear();
	actions.clear();

	FileSystem& fs = FileSystem::inst();
	std::ostringstream timePlayedStr;
	timePlayedStr << timePlayed;
	fs.writeStringToFile(timePlayedStr.str(), fs.getAppHomeDirectory() + ".time_played");

	initialized = false;
}

// #define RECORD
// #define PROFILE_VIDEO_CAPTURE

#ifdef __EMSCRIPTEN__
// A browser tab cannot host a blocking game loop: each frame has to be handed
// back so the page can paint and deliver input. emscripten_set_main_loop calls
// one iteration per animation frame, which means the loop's state can no longer
// live in locals, so it moves to file scope here. (The other option - ASYNCIFY
// plus emscripten_sleep - could not rewind this particular loop reliably.)
namespace
{
	bool   active = true;
	bool   done = false;
	Uint32 timeToProcess = 0;
	uint   timeProcessed = 1;
	uint   firstEventRecorded = ~0u;
}

static void emMainLoopIteration(void* p_engine);
#endif

void Engine::mainLoop()
{
#ifndef __EMSCRIPTEN__
	bool active = true;
	bool done = false;
	Uint32 timeToProcess = 0;
	uint timeProcessed = 1;
	uint firstEventRecorded = ~0;
#endif

	// Cursor-Position abfragen
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
	// do/while(0) so the body's `continue` and `break` still mean
	// "end this frame", exactly as they did inside the real loop.
	do
	{
#else
	do
	{
#endif
		Uint32 start = SDL_GetTicks();

#ifdef __EMSCRIPTEN__
		// Der Browser aendert die Canvas-Groesse, ohne dass SDL 1.2 davon ein
		// SDL_VIDEORESIZE machen wuerde - Groessenaenderung durch das
		// Browserfenster wie durch die Fullscreen-API. Einmal pro Bild
		// nachsehen kostet nichts und faengt beides.
		{
			int canvasWidth = 0, canvasHeight = 0;
			emscripten_get_canvas_element_size("#canvas", &canvasWidth, &canvasHeight);
			if(canvasWidth > 0 && canvasHeight > 0 &&
			   (canvasWidth != displaySize.x || canvasHeight != displaySize.y))
				handleResize(canvasWidth, canvasHeight);
		}
#endif

		// OpenGL-Fehler aufgetreten?
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

		// rendern
		if(active && timeProcessed)
		{
			bindFrameBuffer();
			render();
			frameRendered = true;
		}

		// SDL-Ereignisse verarbeiten
		SDL_Event event;
		while(SDL_PollEvent(&event))
		{
			switch(event.type)
			{
			case SDL_ACTIVEEVENT:
				if(event.active.state & SDL_APPACTIVE || event.active.state & SDL_APPINPUTFOCUS)
				{
					if(event.active.gain)
					{
						if(!active)
						{
							active = true;

							if(oldSoundVolume != -1.0)
							{
								setSoundVolume(oldSoundVolume);
								setMusicVolume(oldMusicVolume);
								oldSoundVolume = -1.0;
							}

							GameState* p_gs = this->getGameState();
							if(p_gs) p_gs->onAppGetFocus();
						}
					}
					else
					{
						if(active)
						{
							oldSoundVolume = soundVolume;
							oldMusicVolume = musicVolume;
							setSoundVolume(0.0);
							setMusicVolume(0.0);
							active = false;

							GameState* p_gs = this->getGameState();
							if(p_gs) p_gs->onAppLoseFocus();

							// Videoaufnahme stoppen, falls gerade eine läuft
							if(p_videoRecorder)
							{
								delete p_videoRecorder;
								p_videoRecorder = 0;
							}
						}
					}
				}
				break;
			case SDL_KEYDOWN:
				// Alt+Return schaltet Vollbild um und wird verschluckt, damit
				// das Spiel darin kein gewöhnliches Return sieht. (Im Browser
				// hat das schon der DOM-Handler erledigt - der Tastendruck
				// kommt trotzdem hier an, und genau deshalb muss er hier weg.)
				if(event.key.keysym.sym == SDLK_RETURN &&
				   (event.key.keysym.mod & KMOD_ALT || SDL_GetModState() & KMOD_ALT))
				{
					swallowedReturn = true;
#ifndef __EMSCRIPTEN__
					toggleFullScreen();
#endif
					break;
				}
				if(event.key.keysym.sym >= 0 && event.key.keysym.sym < NUM_KEY_SLOTS)
					keyData[event.key.keysym.sym] |= (1 | 2);
				keyEventQueue.push(event.key);
				break;
			case SDL_KEYUP:
				// Nicht am Modifikator festmachen: wer Alt vor Return loslaesst,
				// wuerde sonst ein Loslassen ohne Druecken hinterlassen.
				if(event.key.keysym.sym == SDLK_RETURN && swallowedReturn)
				{
					swallowedReturn = false;
					break;
				}
				if(event.key.keysym.sym >= 0 && event.key.keysym.sym < NUM_KEY_SLOTS)
				{
					keyData[event.key.keysym.sym] &= ~1;
					keyData[event.key.keysym.sym] |= 4;
				}
				keyEventQueue.push(event.key);
				break;
			case SDL_MOUSEBUTTONDOWN:
				if(event.button.button < NUM_KEY_SLOTS)
					buttonData[event.button.button] |= (1 | 2);
				break;
			case SDL_MOUSEBUTTONUP:
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
				// Kommt sowohl vom Ziehen am Fensterrand als auch vom
				// Stilwechsel in applyWindowStyle(): SDL bemerkt die neue
				// Groesse ueber sein eigenes WM_WINDOWPOSCHANGED und meldet
				// sie hier. Ein Pfad fuer beides.
				handleResize(event.resize.w, event.resize.h);
				break;
			case SDL_QUIT:
				done = true;
				break;
			}
		}

		if(!active)
		{
			if(!fullScreen) SDL_GL_SwapBuffers();
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

		// bewegen
		timeProcessed = 0;
		while(timeToProcess >= logicRate)
		{
			update();

			if(modal)
			{
				modal = false;
				start = SDL_GetTicks();
			}

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

			// Tastatur- und Mausdaten zurücksetzen
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
				while(!keyEventQueue.empty()) keyEventQueue.pop();
			}

			// Aktionsdaten zurücksetzen
			for(std::unordered_map<std::string, Action*>::const_iterator it = actions.begin();
				it != actions.end();
				++it)
			{
				it->second->data &= ~(2 | 4);
			}

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

		if(crossfadeTime == -0.51)
		{
			// altes Bild sichern
			glBindTexture(GL_TEXTURE_2D, oldImageID);
			glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, screenPow2Size.y - screenSize.y, 0, 0, screenSize.x, screenSize.y);
			crossfadeTime = -0.5;
		}
		else if(crossfadeTime >= -0.5 && frameRendered)
		{
			// aktuelles Bild holen
			glBindTexture(GL_TEXTURE_2D, newImageID);
			glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, screenPow2Size.y - screenSize.y, 0, 0, screenSize.x, screenSize.y);

			// Crossfade rendern
			p_crossfade->render(max(0.0, crossfadeTime / crossfadeDuration), oldImageID, newImageID);
		}

		if(timeProcessed)
		{
			// Crossfade aktualisieren
			if(crossfadeTime >= 0.0)
			{
				crossfadeTime += 0.001 * timeProcessed;
				if(crossfadeTime > crossfadeDuration)
				{
					// Der Crossfade ist vorbei!
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
				// Neues Frame aufnehmen?
				const uint timecode = static_cast<uint>((getExactTimeMS() - recordingStartTime) / (1000.0 / p_videoRecorder->getFPS()));
				if(timecode != lastRecordedFrameTimecode)
				{
					void* p_inputFrameBuffer = p_videoRecorder->getInputFrameBuffer();

#ifdef PROFILE_VIDEO_CAPTURE
					BEGIN_PROFILE(videoCapture)
#endif

					// Bild holen. Immer 640x480 aus dem Bildpuffer, unabhängig von der
					// Fenstergröße - der Videoencoder ist einmal darauf eingerichtet.
					glReadBuffer(useFrameBuffer ? GL_COLOR_ATTACHMENT0_EXT : GL_BACK);
					glReadPixels(0, 0, screenSize.x, screenSize.y, GL_RGBA, GL_UNSIGNED_BYTE, p_inputFrameBuffer);

					if(SDL_ShowCursor(-1))
					{
						// Mauszeiger manuell in den Puffer einzeichnen
						const Vec2i cursorPosition(getCursorPosition());
						for(int dy = 0; dy < 32 && cursorPosition.y + dy < screenSize.y; ++dy)
						{
							for(int dx = 0; dx < 32 && cursorPosition.x + dx < screenSize.x; ++dx)
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

			// Noch vor dem Anzeigen, damit der Screenshot das saubere 640x480-Bild
			// festhält und nicht die skalierte Fassung samt schwarzer Balken.
			if(doScreenshot)
			{
				doScreenshot = false;
				screenshot();
				playSound("screenshot.ogg");
			}

			// Bildpuffer auf den Bildschirm bringen
			unbindFrameBuffer();
			presentFrame();

			// gerendertes Frame anzeigen
			SDL_GL_SwapBuffers();
		}

		Uint32 end = SDL_GetTicks();
		if(frameRendered) frameTime = end - start;

		// warten, wenn noch genug Zeit ist
		uint dt = end - start;
#ifdef __EMSCRIPTEN__
		// This runs inside a requestAnimationFrame callback, which already paces
		// the frame, so there is nothing to wait for - blocking would just stall
		// the page. The logic clock still has to advance by the real time between
		// callbacks though, not by the time spent inside one: on the native path
		// the SDL_Delay below is what makes up that difference, and dropping it
		// without this leaves dt at about a millisecond and runs the game ~20x slow.
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

// #define PROFILE_ENGINE_RENDER

void Engine::render()
{
#ifdef PROFILE_ENGINE_RENDER
	BEGIN_PROFILE(engineRender)
#endif

	// GUI rendern
	GUI::inst().render();

	// Spiel rendern
	GameState* p_gs = getGameState();
	if(p_gs) p_gs->onRender();

	// GUI anzeigen
	GUI::inst().display();

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

	// virtuelle Tasten und Aktionen aktualisieren
	updateVKs();
	updateActions();

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
			// stumm
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
			// Aufnahme stoppen
			delete p_videoRecorder;
			p_videoRecorder = 0;
		}
		else
		{
			FileSystem& fs = FileSystem::inst();
			char videoDateTime[256];
			const time_t t = ::time(0);
			strftime(videoDateTime, 256, "%Y-%m-%d@%H-%M-%S", localtime(&t));
			const std::string filename(FileSystem::inst().getAppHomeDirectory() + "videos/" + videoDateTime + ".mp4");

			// Aufnahme starten
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

	// GUI aktualisieren
	GUI::inst().update();

	processGameStateChanges();

	// Spiel aktualisieren
	GameState* p_gs = getGameState();
	if(p_gs) p_gs->onUpdate();

	processGameStateChanges();

	updateSounds();

	++timePlayed;

#ifdef PROFILE_ENGINE_UPDATE
	END_PROFILE(engineUpdate)
#endif
}

void Engine::updateSounds()
{
	// Sounds aktualisieren
	const std::unordered_multimap<std::string, Sound*>& sounds = Manager<Sound>::inst().getItems();
	for(std::unordered_multimap<std::string, Sound*>::const_iterator i = sounds.begin(); i != sounds.end(); ++i) i->second->update();

	// gestreamte Sounds aktualisieren
	const std::unordered_multimap<std::string, StreamedSound*>& streamedSounds = Manager<StreamedSound>::inst().getItems();
	std::list<StreamedSound*> toBeDeleted;
	for(std::unordered_multimap<std::string, StreamedSound*>::const_iterator i = streamedSounds.begin(); i != streamedSounds.end(); ++i)
	{
		if(!i->second->update())
		{
			toBeDeleted.push_back(i->second);
		}
	}

	// gestoppte Sounds löschen
	for(std::list<StreamedSound*>::const_iterator i = toBeDeleted.begin(); i != toBeDeleted.end(); ++i) (*i)->release();

	if(volumeChanged) volumeChanged = false;
}

std::string Engine::getBestOpenALDevice()
{
	// Standardgerät nehmen
	const char* p_device = alcGetString(0, ALC_DEFAULT_DEVICE_SPECIFIER);
	if(!p_device) return "[NONE]";
	else return p_device;
}

namespace
{
	// Uebersetzt eine Stufe und gibt bei einem Fehler das Log aus.
	uint compileShaderStage(GLenum type, const char* p_source, const char* p_what)
	{
		const uint shader = glExtCreateShader(type);
		if(!shader) return 0;

		glExtShaderSource(shader, 1, &p_source, 0);
		glExtCompileShader(shader);

		GLint ok = 0;
		glExtGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
		if(!ok)
		{
			char log[1024] = "";
			glExtGetShaderInfoLog(shader, sizeof(log) - 1, 0, log);
			printfLog("- WARNING: Could not compile the %s shader: %s\n", p_what, log);
			glExtDeleteShader(shader);
			return 0;
		}
		return shader;
	}
}

bool Engine::createXbrProgram()
{
	const uint vs = compileShaderStage(GL_VERTEX_SHADER, p_xbrVertexShader, "xBR vertex");
	if(!vs) return false;
	const uint fs = compileShaderStage(GL_FRAGMENT_SHADER, p_xbrFragmentShader, "xBR fragment");
	if(!fs) { glExtDeleteShader(vs); return false; }

	xbrProgram = glExtCreateProgram();
	glExtAttachShader(xbrProgram, vs);
	glExtAttachShader(xbrProgram, fs);
	// Feste Plaetze, damit hinterher nichts abgefragt werden muss.
	glExtBindAttribLocation(xbrProgram, 0, "aPosition");
	glExtBindAttribLocation(xbrProgram, 1, "aTexCoord");
	glExtLinkProgram(xbrProgram);

	// Die Stufen haengen jetzt am Programm und werden mit ihm freigegeben.
	glExtDeleteShader(vs);
	glExtDeleteShader(fs);

	GLint ok = 0;
	glExtGetProgramiv(xbrProgram, GL_LINK_STATUS, &ok);
	if(!ok)
	{
		char log[1024] = "";
		glExtGetProgramInfoLog(xbrProgram, sizeof(log) - 1, 0, log);
		printfLog("- WARNING: Could not link the xBR program: %s\n", log);
		destroyXbrProgram();
		return false;
	}

	xbrDecalLocation       = glExtGetUniformLocation(xbrProgram, "decal");
	xbrTextureSizeLocation = glExtGetUniformLocation(xbrProgram, "TextureSize");
	xbrSmallDetailsLocation = glExtGetUniformLocation(xbrProgram, "small_details");

	// WebGL verbietet Vertexdaten aus dem Anwendungsspeicher, es muss ein Puffer
	// sein. Vier Eckpunkte, jedes Bild neu gefuellt - das kostet nichts und
	// erspart es, auf Fenstergroessenaenderungen zu achten.
	glExtGenBuffers(1, &xbrVertexBuffer);
	if(!xbrVertexBuffer)
	{
		printfLog("- WARNING: Could not create the xBR vertex buffer.\n");
		destroyXbrProgram();
		return false;
	}

	return true;
}

void Engine::destroyXbrProgram()
{
	destroySharpFitProgram();
	if(xbrVertexBuffer) { glExtDeleteBuffers(1, &xbrVertexBuffer); xbrVertexBuffer = 0; }
	if(xbrProgram) { glExtDeleteProgram(xbrProgram); xbrProgram = 0; }
	xbrDecalLocation = -1;
	xbrTextureSizeLocation = -1;
	xbrSmallDetailsLocation = -1;
	xbrVertexBuffer = 0;
}

bool Engine::createSharpFitProgram()
{
	// Derselbe Vertex-Shader wie bei xBR, und derselbe Vertexpuffer - nur der
	// Fragment-Shader ist ein anderer, und ein sehr viel billigerer.
	const uint vs = compileShaderStage(GL_VERTEX_SHADER, p_xbrVertexShader, "sharp-fit vertex");
	if(!vs) return false;
	const uint fs = compileShaderStage(GL_FRAGMENT_SHADER, p_sharpFitFragmentShader, "sharp-fit fragment");
	if(!fs) { glExtDeleteShader(vs); return false; }

	sharpFitProgram = glExtCreateProgram();
	glExtAttachShader(sharpFitProgram, vs);
	glExtAttachShader(sharpFitProgram, fs);
	glExtBindAttribLocation(sharpFitProgram, 0, "aPosition");
	glExtBindAttribLocation(sharpFitProgram, 1, "aTexCoord");
	glExtLinkProgram(sharpFitProgram);

	glExtDeleteShader(vs);
	glExtDeleteShader(fs);

	GLint ok = 0;
	glExtGetProgramiv(sharpFitProgram, GL_LINK_STATUS, &ok);
	if(!ok)
	{
		char log[1024] = "";
		glExtGetProgramInfoLog(sharpFitProgram, sizeof(log) - 1, 0, log);
		printfLog("- WARNING: Could not link the sharp-fit program: %s\n", log);
		destroySharpFitProgram();
		return false;
	}

	sharpFitDecalLocation       = glExtGetUniformLocation(sharpFitProgram, "decal");
	sharpFitTextureSizeLocation = glExtGetUniformLocation(sharpFitProgram, "TextureSize");
	sharpFitFrameSizeLocation   = glExtGetUniformLocation(sharpFitProgram, "FrameSize");
	sharpFitPrescaleLocation    = glExtGetUniformLocation(sharpFitProgram, "Prescale");
	return true;
}

void Engine::destroySharpFitProgram()
{
	if(sharpFitProgram) { glExtDeleteProgram(sharpFitProgram); sharpFitProgram = 0; }
	sharpFitDecalLocation = -1;
	sharpFitTextureSizeLocation = -1;
	sharpFitFrameSizeLocation = -1;
	sharpFitPrescaleLocation = -1;
}

void Engine::setUpscaleFilter(UpscaleFilter filter)
{
	// Nur merken. Ob xBR wirklich geht, entscheidet getEffectiveUpscaleFilter()
	// bei jedem Bild neu - beim Laden der config.xml gibt es noch gar keinen
	// GL-Kontext, gegen den man hier pruefen koennte.
	upscaleFilter = filter;
}

const char* Engine::getUpscaleFilterName(UpscaleFilter filter)
{
	switch(filter)
	{
	case UF_NEAREST:    return "nearest";
	case UF_SHARP_FIT:  return "sharp-fit";
	case UF_XBR:        return "xbr";
	case UF_XBR_DETAIL: return "xbr-details";
	default:            return "bilinear";
	}
}

Engine::UpscaleFilter Engine::parseUpscaleFilterName(const char* p_name, UpscaleFilter fallback)
{
	if(!p_name) return fallback;
	if(!_stricmp(p_name, "nearest"))     return UF_NEAREST;
	if(!_stricmp(p_name, "bilinear"))    return UF_BILINEAR;
	if(!_stricmp(p_name, "sharp-fit"))   return UF_SHARP_FIT;
	if(!_stricmp(p_name, "xbr"))         return UF_XBR;
	if(!_stricmp(p_name, "xbr-details")) return UF_XBR_DETAIL;
	return fallback;
}

bool Engine::canUseXbr() const
{
	return useFrameBuffer && xbrProgram != 0 && xbrVertexBuffer != 0;
}

bool Engine::canUseSharpFit() const
{
	return useFrameBuffer && sharpFitProgram != 0 && xbrVertexBuffer != 0;
}

Engine::UpscaleFilter Engine::getEffectiveUpscaleFilter() const
{
	// Ohne uebersetztes Programm gibt es kein xBR - dann lieber bilinear als
	// ein Bild, das gar nicht erst erscheint.
	if((upscaleFilter == UF_XBR || upscaleFilter == UF_XBR_DETAIL) && !canUseXbr())
		return UF_BILINEAR;
	if(upscaleFilter == UF_SHARP_FIT && !canUseSharpFit())
		return UF_NEAREST;   // ohne Shader lieber scharf als weich
	return upscaleFilter;
}

bool Engine::createFrameBuffer()
{
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

	// Der Sternenwischer (cf_star.cpp) und die Lichtmaske in level.cpp brauchen
	// einen Stencil-Puffer. Ein reiner Farbpuffer wäre also zu wenig.
	glExtGenRenderbuffers(1, &frameDepthStencilID);
	glExtBindRenderbuffer(GL_RENDERBUFFER_EXT, frameDepthStencilID);
#ifdef __EMSCRIPTEN__
	// WebGL 1 kennt genau ein kombiniertes Format und einen kombinierten
	// Anhängepunkt dafür.
	glExtRenderbufferStorage(GL_RENDERBUFFER_EXT, GL_DEPTH_STENCIL_EXT,
							 frameTextureSize.x, frameTextureSize.y);
	glExtFramebufferRenderbuffer(GL_FRAMEBUFFER_EXT, GL_DEPTH_STENCIL_ATTACHMENT_EXT,
								 GL_RENDERBUFFER_EXT, frameDepthStencilID);
#else
	// EXT_packed_depth_stencil kennt keinen kombinierten Anhängepunkt: derselbe
	// Renderbuffer wird an beide gehängt, so schreibt es die Spezifikation vor.
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
	if(frameDepthStencilID) { glExtDeleteRenderbuffers(1, &frameDepthStencilID); frameDepthStencilID = 0; }
	if(frameBufferID)       { glExtDeleteFramebuffers(1, &frameBufferID);        frameBufferID = 0; }
	if(frameTextureID)      { glDeleteTextures(1, &frameTextureID);              frameTextureID = 0; }
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
// Der Browser gibt Vollbild nur her, wenn ein echter Klick oder Tastendruck es
// ausloest. SDLs Ereignisse kommen aber aus der Animationsschleife und zaehlen
// nicht als solcher, deshalb haengt Alt+Return hier direkt am DOM.
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
	if(fullScreen)
	{
		EmscriptenFullscreenStrategy strategy;
		memset(&strategy, 0, sizeof(strategy));
		// Der Canvas bekommt die volle Bildschirmgroesse; die schwarzen Balken
		// zeichnet presentFrame selbst, genau wie im Fenster.
		strategy.scaleMode = EMSCRIPTEN_FULLSCREEN_SCALE_STRETCH;
		strategy.canvasResolutionScaleMode = EMSCRIPTEN_FULLSCREEN_CANVAS_SCALE_STDDEF;
		strategy.filteringMode = EMSCRIPTEN_FULLSCREEN_FILTERING_DEFAULT;
		emscripten_request_fullscreen_strategy("#canvas", EM_TRUE, &strategy);
	}
	else emscripten_exit_fullscreen();
}
#endif

Vec2i Engine::getDesktopSize() const
{
#ifdef __EMSCRIPTEN__
	// Im Browser ist "Desktop" das Fenster, in dem die Seite steht.
	return Vec2i(EM_ASM_INT({ return window.innerWidth | 0; }),
				 EM_ASM_INT({ return window.innerHeight | 0; }));
#else
	// SDL_ListModes(0, SDL_FULLSCREEN) waere die portable Variante, liefert aber
	// die Modusliste und nicht den Desktop. SDL_GetVideoInfo liefert vor dem
	// ersten SDL_SetVideoMode die Desktopaufloesung - danach die des Fensters,
	// deshalb wird sie unter Win32 direkt erfragt.
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

void Engine::applyWindowStyle(bool wantFullScreen, const Vec2i& size)
{
	// SDLs Flags werden hier bewusst nicht angefasst. SDL_FULLSCREEN oder
	// SDL_NOFRAME zu setzen wuerde DIB_SetVideoMode auf den langsamen Pfad
	// zwingen, und der ruft WIN_GL_ShutDown - der GL-Kontext und mit ihm jede
	// Textur, jede Displayliste und der Bildpuffer waeren weg. Stattdessen wird
	// direkt der Win32-Stil umgeschaltet; SDL merkt die neue Groesse ueber sein
	// eigenes WM_WINDOWPOSCHANGED und schickt ein ganz normales SDL_VIDEORESIZE.
#ifdef _WIN32
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if(SDL_GetWMInfo(&info) && info.window)
	{
		HWND hwnd = info.window;
		if(wantFullScreen)
		{
			// Stil und Rechteck merken, damit das Fenster genau dorthin
			// zurueckkommt, wo es war - aber nur beim ersten Mal. Wer hier ein
			// zweites Mal hineinlaeuft, wuerde sonst den bereits gesetzten
			// WS_POPUP als "das war vorher" merken, und dann fuehrt aus dem
			// Vollbild kein Weg mehr heraus.
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
			// HWND_TOP, nicht HWND_TOPMOST: ein randloses Vollbildfenster, das
			// ueber allem klebt, macht Alt+Tab unbrauchbar.
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
				// Nichts gemerkt - trotzdem zurueck ins Fenster. Aus dem
				// Vollbild muss man immer wieder herauskommen, auch wenn
				// unterwegs etwas schiefgegangen ist.
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
#endif

	// Immer hierdurch: displaySize gehoert handleResize, und SDL muss die neue
	// Groesse ohnehin erfahren - sonst bleibt der Mauszeiger auf den alten
	// Bereich geklemmt. Das SDL_VIDEORESIZE, das der Stilwechsel gleich noch
	// ausloest, findet dann nichts mehr zu tun und faellt sofort wieder heraus.
	handleResize(size.x, size.y);
}

void Engine::setFullScreen(bool wantFullScreen)
{
	if(!initialized || fullScreen == wantFullScreen) { fullScreen = wantFullScreen; return; }

	fullScreen = wantFullScreen;
	printfLog("* %s\n", wantFullScreen ? "Going fullscreen" : "Leaving fullscreen");

#ifdef __EMSCRIPTEN__
	// Im Browser macht das die Fullscreen-API. Aufgerufen wird das hier nur aus
	// engineFullScreenHotkey(), das am DOM haengt - aus der Spielschleife
	// heraus wuerde der Browser die Anfrage ablehnen.
	emscriptenSetFullScreen(wantFullScreen);
#else
	applyWindowStyle(wantFullScreen, wantFullScreen ? getDesktopSize() : windowedSize);
#endif
}

void Engine::handleResize(int width, int height)
{
#ifndef __EMSCRIPTEN__
	// Kleiner als das interne Bild darf das Fenster nicht werden: darunter hat
	// "Scharf" keine ganzzahlige Stufe mehr. Im Browser wird nicht geklemmt -
	// dort gibt der Canvas die Groesse vor, und dagegen anzuschieben endete in
	// einer Endlosschleife.
	if(width  < screenSize.x) width  = screenSize.x;
	if(height < screenSize.y) height = screenSize.y;
#endif

	if(width <= 0 || height <= 0) return;
	if(width == displaySize.x && height == displaySize.y) return;

#ifndef __EMSCRIPTEN__
	// Gleiche Flags wie beim ersten Mal, sonst geht der schnelle Pfad verloren.
	SDL_Surface* p_new = SDL_SetVideoMode(width, height, 32, SDL_OPENGL | SDL_RESIZABLE);
	if(!p_new)
	{
		printfLog("- WARNING: Could not resize to %dx%d (%s).\n", width, height, SDL_GetError());
		return;
	}
	p_display = p_new;
#endif

	displaySize = Vec2i(width, height);
	if(!fullScreen) windowedSize = displaySize;
}

void Engine::computePresentRect(int& x, int& y, int& w, int& h) const
{
	// Größtmögliches 4:3-Rechteck im Fenster, mittig. Was übrig bleibt, wird
	// schwarz - lieber Balken als ein verzerrtes Bild.
	double scale = min(static_cast<double>(displaySize.x) / screenSize.x,
					   static_cast<double>(displaySize.y) / screenSize.y);

	// "Scharf" braucht eine ganzzahlige Stufe. Bei einem krummen Faktor
	// verdoppelt Nearest manche Quellpixel und andere nicht - die Sprites
	// bekommen ungleiche Strichstärken und die Schrift wird fransig, genau der
	// Fehler, den dieser Filter vermeiden soll. Lieber breitere Balken.
	// Unterhalb von 1:1 gibt es keine Stufe mehr, dann eben doch krumm: ein
	// abgeschnittenes Bild wäre schlimmer.
	if(getEffectiveUpscaleFilter() == UF_NEAREST && scale >= 1.0) scale = floor(scale);

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

	// Benutzt wird nur die linke untere Ecke der Zweierpotenz-Textur.
	const double u = static_cast<double>(screenSize.x) / frameTextureSize.x;
	const double v = static_cast<double>(screenSize.y) / frameTextureSize.y;

	glBindTexture(GL_TEXTURE_2D, frameTextureID);

	// Nearest und Bilinear sind reine Filtereinstellungen der Textur, dafuer
	// braucht es keinen Shader. xBR braucht ebenfalls GL_NEAREST: der Shader
	// rekonstruiert die Kanten selbst aus exakten Texeln, und mit bilinear
	// vorgemischten Nachbarn findet seine Kantenerkennung nichts mehr - das
	// Ergebnis ist dann kaum von bilinear zu unterscheiden.
	const UpscaleFilter effective = getEffectiveUpscaleFilter();
	// UF_SHARP_FIT rechnet die Texturkoordinate so um, dass die
	// Hardware-Interpolation genau das nearest-Ergebnis liefert - ohne sie
	// bliebe von dem Filter nichts uebrig. xBR braucht das Gegenteil.
	const GLint filter = (effective == UF_BILINEAR || effective == UF_SHARP_FIT)
						 ? GL_LINEAR : GL_NEAREST;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

	if(effective == UF_XBR || effective == UF_XBR_DETAIL || effective == UF_SHARP_FIT)
	{
		// Der Shader rechnet selbst in Clipkoordinaten - keine Matrix, kein
		// Anfassen des Fixed-Function-Zustands, und im Browser damit auch keine
		// Beruehrung mit Emscriptens Immediate-Mode-Nachbau.
		const float x0 = 2.0f * x           / displaySize.x - 1.0f;
		const float x1 = 2.0f * (x + w)     / displaySize.x - 1.0f;
		const float y0 = 2.0f * y           / displaySize.y - 1.0f;
		const float y1 = 2.0f * (y + h)     / displaySize.y - 1.0f;
		const float fu = static_cast<float>(u), fv = static_cast<float>(v);

		// Zwei Dreiecke als Streifen: Position, dann Texturkoordinate.
		const float vertices[16] =
		{
			x0, y0, 0.0f, 0.0f,
			x1, y0, fu,   0.0f,
			x0, y1, 0.0f, fv,
			x1, y1, fu,   fv
		};

		if(effective == UF_SHARP_FIT)
		{
			// Der kleinste ganzzahlige Faktor, mit dem das 640x480-Bild das
			// Zielrechteck mindestens ausfuellt. Genau um den wuerde man mit
			// nearest vergroessern, bevor man auf die echte Groesse
			// heruntergeht; der Shader macht beides in einem Zug.
			const float prescaleX = static_cast<float>(max(1, static_cast<int>(ceil(static_cast<double>(w) / screenSize.x))));
			const float prescaleY = static_cast<float>(max(1, static_cast<int>(ceil(static_cast<double>(h) / screenSize.y))));

			glExtUseProgram(sharpFitProgram);
			if(sharpFitDecalLocation >= 0)       glExtUniform1i(sharpFitDecalLocation, 0);
			if(sharpFitTextureSizeLocation >= 0) glExtUniform2f(sharpFitTextureSizeLocation,
																static_cast<float>(frameTextureSize.x),
																static_cast<float>(frameTextureSize.y));
			if(sharpFitFrameSizeLocation >= 0)   glExtUniform2f(sharpFitFrameSizeLocation,
																static_cast<float>(screenSize.x),
																static_cast<float>(screenSize.y));
			if(sharpFitPrescaleLocation >= 0)    glExtUniform2f(sharpFitPrescaleLocation, prescaleX, prescaleY);
		}
		else
		{
			glExtUseProgram(xbrProgram);
			if(xbrDecalLocation >= 0)       glExtUniform1i(xbrDecalLocation, 0);
			if(xbrTextureSizeLocation >= 0) glExtUniform2f(xbrTextureSizeLocation,
														   static_cast<float>(frameTextureSize.x),
														   static_cast<float>(frameTextureSize.y));
			// small_details bestimmt, woran der Shader zwei Texel als "gleich"
			// erkennt: 0 = gewichtete Mischung aus Helligkeit und Farbe, so wie
			// xBR es normalerweise macht; 1 = nur die Helligkeit. Damit greift
			// er auch in gerasterten Flaechen (Gras, Erde), die sonst pixelig
			// bleiben.
			if(xbrSmallDetailsLocation >= 0)
				glExtUniform1f(xbrSmallDetailsLocation,
							   effective == UF_XBR_DETAIL ? 1.0f : 0.0f);
		}

		glExtBindBuffer(GL_ARRAY_BUFFER, xbrVertexBuffer);
		glExtBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STREAM_DRAW);
		glExtEnableVertexAttribArray(0);
		glExtEnableVertexAttribArray(1);
		glExtVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<const void*>(0));
		glExtVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<const void*>(2 * sizeof(float)));

		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		glExtDisableVertexAttribArray(0);
		glExtDisableVertexAttribArray(1);
		glExtBindBuffer(GL_ARRAY_BUFFER, 0);
		glExtUseProgram(0);
	}
	else
	{
		glBegin(GL_QUADS);
		glTexCoord2d(0.0, 0.0); glVertex2i(x,     y);
		glTexCoord2d(u,   0.0); glVertex2i(x + w, y);
		glTexCoord2d(u,   v);   glVertex2i(x + w, y + h);
		glTexCoord2d(0.0, v);   glVertex2i(x,     y + h);
		glEnd();
	}

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
}

void Engine::screenshot()
{
#ifdef __EMSCRIPTEN__
	// GL_BGR is not an accepted glReadPixels format in WebGL 1, and Emscripten
	// implements SDL_SaveBMP_RW as abort(), so this path cannot work as written
	// and taking it down the old route would kill the runtime. Saving a file from
	// a browser needs a download anyway, which is a separate piece of work.
	printfLog("Screenshots are not supported in the web build.\n");
	return;
#endif
	// Immer das interne 640x480-Bild, nie die skalierte Fassung: der gewählte
	// Filter ist eine Anzeigeeinstellung und gehört nicht in die Datei, und
	// schwarze Balken erst recht nicht.
	const Vec2i shotSize(useFrameBuffer ? screenSize : displaySize);

	char* p_temp = new char[shotSize.x * shotSize.y * 3];

	glReadBuffer(useFrameBuffer ? GL_COLOR_ATTACHMENT0_EXT : GL_BACK);
	glReadPixels(0, 0, shotSize.x, shotSize.y, GL_BGR, GL_UNSIGNED_BYTE, p_temp);

	// Zeilen richtigherum drehen
	char* p_buffer = new char[shotSize.x * shotSize.y * 3];
	char* p_cursor = p_temp;
	for(int y = 0; y < shotSize.y; y++)
	{
		int ny = shotSize.y - 1 - y;
		memcpy(p_buffer + shotSize.x * 3 * ny, p_cursor, shotSize.x * 3);
		p_cursor += shotSize.x * 3;
	}

	delete[] p_temp;

	SDL_Surface* p_surface = SDL_CreateRGBSurfaceFrom(p_buffer, shotSize.x, shotSize.y, 24, shotSize.x * 3, 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00000000);

	FileSystem& fs = FileSystem::inst();
	char screenshotDateTime[256];
	const time_t t = ::time(0);
	strftime(screenshotDateTime, 256, "%Y-%m-%d@%H-%M-%S", localtime(&t));
	std::string filename;
	for(uint no = 1; true; no++)
	{
		char temp[512] = "";
		if(no == 1) sprintf(temp, "%s.bmp", screenshotDateTime);
		else sprintf(temp, "%s_%02d.bmp", screenshotDateTime, no);
		filename = FileSystem::inst().getAppHomeDirectory() + "screenshots/" + temp;
		if(!fs.fileExists(filename)) break;
	}

	SDL_SaveBMP(p_surface, filename.c_str());

	SDL_FreeSurface(p_surface);
	delete[] p_buffer;
}

void Engine::renderSprite(const Vec2i& position,
						  const Vec2i& positionOnTexture,
						  const Vec2i& size,
						  const Vec4d& color,
						  bool mirrorX,
						  double rotation,
						  double scaling)
{
	const Vec2i halfSize(size / 2);

	glPushMatrix();
	glTranslated(position.x + halfSize.x, position.y + halfSize.y, 0.0);
	if(scaling != 1.0) glScaled(scaling, scaling, 1.0);
	if(rotation != 0.0) glRotated(rotation, 0.0, 0.0, 1.0);
	if(mirrorX) glScaled(-1.0, 1.0, 1.0);

	glBegin(GL_QUADS);
	glColor4dv(color);
	glTexCoord2i(positionOnTexture.x, positionOnTexture.y);
	glVertex2i(-halfSize.x, -halfSize.y);
	glTexCoord2i(positionOnTexture.x + size.x, positionOnTexture.y);
	glVertex2i(halfSize.x, -halfSize.y);
	glTexCoord2i(positionOnTexture.x + size.x, positionOnTexture.y + size.y);
	glVertex2i(halfSize.x, halfSize.y);
	glTexCoord2i(positionOnTexture.x, positionOnTexture.y + size.y);
	glVertex2i(-halfSize.x, halfSize.y);
	glEnd();

	glPopMatrix();
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
			// Höhe setzen
			if(pitchSpectrum != 0.0) p_inst->setPitch(1.0 + random(-pitchSpectrum, pitchSpectrum));

			// Priorität setzen
			p_inst->setPriority(priority);

			// abspielen
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

	// aktueller Zustand verliert den Fokus
	p_stateToLoseFocus = getGameState();

	// alle Zustände verlassen
	while(!currentGameStates.empty())
	{
		GameState* p_gs = currentGameStates.top();
		statesToBeLeft.push_back(p_gs);
		currentGameStates.pop();
	}

	if(p_newGS)
	{
		// neuen Zustand betreten
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

	// aktueller Zustand verliert den Fokus
	GameState* p_currentGS = getGameState();
	if(p_currentGS) p_stateToLoseFocus = p_currentGS;

	// neuen Zustand betreten
	currentGameStates.push(p_newGS);
	p_stateToBeEntered = p_newGS;
	p_stateToGetFocus = p_newGS;
}

GameState* Engine::popGameState(const ParameterBlock& context)
{
	GameState* p_currentGS = getGameState();
	if(p_currentGS)
	{
		// aktuellen Zustand verlassen
		currentGameStates.pop();
		GameState* p_newGS = getGameState();
		p_stateToLoseFocus = p_currentGS;
		statesToBeLeft.push_back(p_currentGS);

		// neuer Zustand erhält den Fokus
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
	// Zustandswechsel vollziehen
	if(p_stateToLoseFocus) p_stateToLoseFocus->onLoseFocus();
	while(!statesToBeLeft.empty())
	{
		statesToBeLeft.front()->onLeave(context);
		statesToBeLeft.erase(statesToBeLeft.begin());
	}

	if(p_stateToBeEntered) p_stateToBeEntered->onEnter(context);
	if(p_stateToGetFocus) p_stateToGetFocus->onGetFocus();

	p_stateToBeEntered = p_stateToGetFocus = p_stateToLoseFocus = 0;
}

void Engine::playMusic(const std::string& filename,
					   double loopBegin)
{
	// Muss die Musik gewechselt werden?
	if(currentMusicFilename != filename)
	{
		stopMusic();

		currentMusicFilename = filename;

		if(!filename.empty())
		{
			// neue Musik laden
			p_currentMusic = Manager<StreamedSound>::inst().request(filename);
			if(p_currentMusic)
			{
				p_currentMusic->setVolume(0.0);
				p_currentMusic->play(loopBegin != -1.0);
				p_currentMusic->slideVolume(1.0, 0.02);
				p_currentMusic->setLoopBegin(loopBegin);
			}
		}
	}
}

void Engine::stopMusic()
{
	if(p_currentMusic)
	{
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

bool Engine::wasKeyReleased(SDLKey key) const
{
	if(key < 0 || key >= NUM_KEY_SLOTS) return false;
	return keyData[key] & 4 ? true : false;
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

bool Engine::getKeyEvent(SDL_KeyboardEvent* p_out)
{
	if(keyEventQueue.empty()) return false;
	else
	{
		*p_out = keyEventQueue.front();
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

Action* Engine::registerAction(const std::string& name,
							   int primary,
							   int secondary)
{
	Action* p_action = new Action;
	p_action->name = name;
	p_action->primary = primary;
	p_action->secondary = secondary;
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
	// Tastatur und Joysticks abfragen
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
			// Taste
			vk.down = p_keys[vk.key] ? true : false;
		}
		else
		{
			// Joystick
			SDL_Joystick* p_joystick = joysticks[vk.device];
			if(vk.key != -1)
			{
				// Knopf
				vk.down = SDL_JoystickGetButton(p_joystick, vk.key) ? true : false;
			}
			else if(vk.axis != -1)
			{
				// Achse
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
				// Hat
				vk.down = SDL_JoystickGetHat(p_joystick, vk.hat) == vk.hatDir;
			}
		}
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
			// gedrückt
			if(!a.countDown)
			{
				a.data |= 2;
				a.countDown = a.delay;

				// entgegengesetzte Aktionen zurücksetzen
				for(std::vector<std::string>::const_iterator jt = a.resetsActions.begin();
					jt != a.resetsActions.end();
					++jt)
				{
					Action* p_reset = getAction(*jt);
					if(p_reset && p_reset->data & 1) p_reset->data |= 8;
				}
			}
			else if(a.buffered < 5)
			{
				// puffern
				++a.buffered;
				if(a.countDown > a.interval) a.countDown = a.interval;
			}
		}
		else if(!down && oldDown)
		{
			// losgelassen
			a.data |= 4;
			a.data &= ~8;

			// entgegengesetzte Aktionen wieder aktivieren
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
			// gedrückt und vorher auch gedrückt
			if(!a.countDown)
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

				// Ist noch etwas gepuffert?
				if(a.buffered)
				{
					--a.buffered;

					a.data = 1 | 2 | 4;
					a.countDown = a.interval;

					// entgegengesetzte Aktionen zurücksetzen
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

int Engine::getPressedVK(int timeOut)
{
	Uint32 end = SDL_GetTicks() + timeOut;
	modal = true;

	updateVKs();
	std::vector<bool> oldState;
	for(size_t i = 0; i < virtualKeys.size(); i++) oldState.push_back(virtualKeys[i].down);

	while(timeOut == -1 || SDL_GetTicks() < end)
	{
		updateVKs();
		if(virtualKeys[getKeyboardVK(SDLK_ESCAPE)].down) return -1;

		for(size_t i = 0; i < virtualKeys.size(); i++)
		{
			if(virtualKeys[i].down && !oldState[i]) return static_cast<int>(i);
		}

		SDL_Delay(10);
	}

	return -1;
}

void Engine::resetActions()
{
	for(size_t i = 0; i < actionsVector.size(); i++)
	{
		actionsVector[i]->primary = actionsVector[i]->defaultPrimary;
		actionsVector[i]->secondary = actionsVector[i]->defaultSecondary;
	}
}

void Engine::limitActionKeys()
{
	// Indizes der Aktionen limitieren
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
		// Genau die Umkehrung dessen, was presentFrame() zeichnet. Das Rechteck
		// liegt mittig, deshalb ist der Rand oben so groß wie unten und die
		// Rechnung gilt gleichermaßen in SDLs Fensterkoordinaten (y nach unten)
		// wie in GLs (y nach oben).
		int x, y, w, h;
		computePresentRect(x, y, w, h);
		if(w > 0 && h > 0)
		{
			position.x = static_cast<int>((position.x - x) * static_cast<double>(screenSize.x) / w);
			position.y = static_cast<int>((position.y - y) * static_cast<double>(screenSize.y) / h);
		}
	}

	position = Vec2i(clamp(position.x, 0, screenSize.x - 1),
					 clamp(position.y, 0, screenSize.y - 1));

	return position;
}

void Engine::setCursorPosition(const Vec2i& cursorPosition)
{
	// Erst in den gültigen Bereich des internen Bildes, dann nach außen
	// umrechnen. Die alte Fassung klemmte mit clamp(temp.x, 0, temp.x - 1) auf
	// eine Grenze, die aus dem geklemmten Wert selbst stammte, und zog dadurch
	// immer genau eins ab.
	Vec2i temp = Vec2i(clamp(cursorPosition.x, 0, screenSize.x - 1),
					   clamp(cursorPosition.y, 0, screenSize.y - 1));

	if(useFrameBuffer)
	{
		int x, y, w, h;
		computePresentRect(x, y, w, h);
		if(screenSize.x > 0 && screenSize.y > 0)
		{
			temp.x = x + static_cast<int>(temp.x * static_cast<double>(w) / screenSize.x);
			temp.y = y + static_cast<int>(temp.y * static_cast<double>(h) / screenSize.y);
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
		// Crossfade abbrechen
		delete this->p_crossfade;
		this->p_crossfade = 0;
		crossfadeTime = -1.0;
		crossfadeDuration = 0.0;
	}
	else
	{
		// Crossfade starten
		this->p_crossfade = p_crossfade;
		crossfadeTime = -0.51;
		crossfadeDuration = duration;
	}

	if(immediately)
	{
		// altes Bild sichern
		glBindTexture(GL_TEXTURE_2D, oldImageID);
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, screenPow2Size.y - screenSize.y, 0, 0, screenSize.x, screenSize.y);
		crossfadeTime = -0.5;
	}
}

void Engine::loadConfig()
{
	language = "en";
	soundVolume = musicVolume = 1.0;
	particleDensity = 1.0;
	details = 2;

	TiXmlDocument doc;
	doc.LoadFile(FileSystem::inst().getAppHomeDirectory() + "config.xml");
	if(doc.ErrorId()) return;

	TiXmlElement* p_config = doc.FirstChildElement("Config");
	if(p_config)
	{
		// Sprache lesen
		TiXmlElement* p_language = p_config->FirstChildElement("Language");
		if(p_language)
		{
			const char* p_text = p_language->GetText();
			if(p_text) setLanguage(p_text);
		}

		// Skalierungsfilter lesen. Steht nichts da, bleibt die Voreinstellung.
		// Ob xBR wirklich geht, entscheidet getEffectiveUpscaleFilter() später -
		// hier gibt es noch keinen GL-Kontext.
		TiXmlElement* p_upscaler = p_config->FirstChildElement("Upscaler");
		if(p_upscaler) upscaleFilter = parseUpscaleFilterName(p_upscaler->GetText(), upscaleFilter);

		// Vollbild und Fenstergröße lesen. Beide gelten erst beim nächsten
		// Start; mitten im Betrieb schaltet der Spieler mit Alt+Return und dem
		// Fensterrahmen selbst.
#ifndef __EMSCRIPTEN__
		TiXmlElement* p_fullScreen = p_config->FirstChildElement("Fullscreen");
		if(p_fullScreen)
		{
			const char* p_text = p_fullScreen->GetText();
			if(p_text) fullScreen = (atoi(p_text) != 0);
		}
#endif

		TiXmlElement* p_windowSize = p_config->FirstChildElement("WindowSize");
		if(p_windowSize)
		{
			int w = 0, h = 0;
			p_windowSize->QueryIntAttribute("w", &w);
			p_windowSize->QueryIntAttribute("h", &h);
			// Kleiner als das interne Bild ergibt keinen Sinn, und eine
			// unsinnig große Zahl aus einer verbogenen Datei auch nicht.
			if(w >= screenSize.x && h >= screenSize.y && w <= 16384 && h <= 16384)
				windowedSize = Vec2i(w, h);
		}

		// Sound-Lautstärke lesen
		TiXmlElement* p_soundVolume = p_config->FirstChildElement("SoundVolume");
		if(p_soundVolume)
		{
			const char* p_text = p_soundVolume->GetText();
			if(p_text) setSoundVolume(atof(p_text));
		}

		// Musik-Lautstärke lesen
		TiXmlElement* p_musicVolume = p_config->FirstChildElement("MusicVolume");
		if(p_musicVolume)
		{
			const char* p_text = p_musicVolume->GetText();
			if(p_text) setMusicVolume(atof(p_text));
		}

		// Details lesen
		TiXmlElement* p_details = p_config->FirstChildElement("Details");
		if(p_details)
		{
			const char* p_text = p_details->GetText();
			if(p_text) setDetails(atoi(p_text));
		}

		// Steuerung lesen
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
						int primary = -1, secondary = -1;
						p_action->QueryIntAttribute("primary", &primary);
						p_action->QueryIntAttribute("secondary", &secondary);
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

	if(!virtualKeys.empty()) limitActionKeys();
}

void Engine::saveConfig()
{
	TiXmlDocument doc;

	TiXmlDeclaration* p_decl = new TiXmlDeclaration("1.0", "", "");
	doc.LinkEndChild(p_decl);

	TiXmlElement* p_config = new TiXmlElement("Config");

	// Sprache schreiben
	TiXmlElement* p_language = new TiXmlElement("Language");
	p_language->LinkEndChild(new TiXmlText(language));
	p_config->LinkEndChild(p_language);

	// Skalierungsfilter schreiben
	TiXmlElement* p_upscaler = new TiXmlElement("Upscaler");
	p_upscaler->LinkEndChild(new TiXmlText(getUpscaleFilterName(upscaleFilter)));
	p_config->LinkEndChild(p_upscaler);

	// Vollbild und Fenstergröße schreiben, damit das Spiel so wiederkommt,
	// wie es verlassen wurde.
	TiXmlElement* p_fullScreen = new TiXmlElement("Fullscreen");
	p_fullScreen->LinkEndChild(new TiXmlText(fullScreen ? "1" : "0"));
	p_config->LinkEndChild(p_fullScreen);

	TiXmlElement* p_windowSize = new TiXmlElement("WindowSize");
	p_windowSize->SetAttribute("w", windowedSize.x);
	p_windowSize->SetAttribute("h", windowedSize.y);
	p_config->LinkEndChild(p_windowSize);

	// Sound-Lautstärke schreiben
	TiXmlElement* p_soundVolume = new TiXmlElement("SoundVolume");
	char temp[256] = "";
	sprintf(temp, "%f", getSoundVolume());
	p_soundVolume->LinkEndChild(new TiXmlText(temp));
	p_config->LinkEndChild(p_soundVolume);

	// Musik-Lautstärke schreiben
	TiXmlElement* p_musicVolume = new TiXmlElement("MusicVolume");
	sprintf(temp, "%f", getMusicVolume());
	p_musicVolume->LinkEndChild(new TiXmlText(temp));
	p_config->LinkEndChild(p_musicVolume);

	// Details schreiben
	TiXmlElement* p_details = new TiXmlElement("Details");
	sprintf(temp, "%d", getDetails());
	p_details->LinkEndChild(new TiXmlText(temp));
	p_config->LinkEndChild(p_details);

	// Steuerung schreiben
	TiXmlElement* p_controls = new TiXmlElement("Controls");
	for(size_t i = 0; i < actionsVector.size(); i++)
	{
		TiXmlElement* p_action = new TiXmlElement("Action");
		p_action->SetAttribute("name", actionsVector[i]->name.c_str());
		p_action->SetAttribute("primary", actionsVector[i]->primary);
		p_action->SetAttribute("secondary", actionsVector[i]->secondary);
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
			// Zeile ist fertig!

			if(line.empty())
			{
				// Eine leere Zeile wird gespeichert, wenn sie nicht am Anfang steht.
				if(!texts.empty()) numEmptyLines++;
			}
			else if(line.find_first_of("//") == 0)
			{
				// Es ist nur ein Kommentar.
			}
			else
			{
				if(line[0] == '$')
				{
					// Das ist die String-ID!

					// zuerst den alten String abspeichern
					stringDB[id] = texts;

					// von vorne anfangen
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
					// Das ist eine Textzeile!
					if(texts.empty()) texts = line;
					else
					{
						if(line[0] == '§')
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

			// \n nach \r überspringen
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
	if(!text.empty())
	{
		if(text[0] == '$')
		{
			// Gibt es diesen String in der Datenbank?
			std::unordered_map<std::string, std::string>::const_iterator i = stringDB.find(text);
			if(i != stringDB.end())
			{
				// Ja! Lokalisieren!
				return localizeString(i->second);
			}
		}
	}

	// Suchmuster generieren
	const std::string patternStart = std::string("§") + language + std::string(":");

	std::string::size_type indexStart = text.find(patternStart);
	if(std::string::npos == indexStart)
	{
		// Keine Lokalisierung für diese Sprache!
		if(language == "en")
		{
			// String unverändert liefern
			return text;
		}
		else
		{
			// versuchen wir's noch einmal auf Englisch ...
			std::string oldLanguage = language;
			language = "en";
			std::string result = localizeString(text);
			language = oldLanguage;
			return result;
		}
	}

	std::string::size_type textStart = indexStart + language.length() + 2;

	std::string::size_type textEnd = text.find("§", textStart);
	if(std::string::npos == textEnd)
	{
		// Dies war die letzte Lokalisierung.
		return text.substr(textStart);
	}

	return text.substr(textStart, textEnd - textStart);
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
	const char* p_arrow[] = {
		/* width height num_colors chars_per_pixel */
		"    32    32        3            1",
		/* colors */
		"X c #000000",
		". c #ffffff",
		"  c None",
		/* pixels */
		"X                               ",
		"XX                              ",
		"X.X                             ",
		"X..X                            ",
		"X...X                           ",
		"X....X                          ",
		"X.....X                         ",
		"X......X                        ",
		"X.......X                       ",
		"X.....XXX                       ",
		"X..X..X                         ",
		"X.X X..X                        ",
		"XX  X..X                        ",
		"     X..X                       ",
		"     X..X                       ",
		"      XX                        ",
		"                                ",
		"                                ",
		"                                ",
		"                                ",
		"                                ",
		"                                ",
		"                                ",
		"                                ",
		"                                ",
		"                                ",
		"                                ",
		"                                ",
		"                                ",
		"                                ",
		"                                ",
		"                                ",
		"0,0"
	};

	int i, row, col;
	Uint8 data[4*32];
	Uint8 mask[4*32];
	int hot_x, hot_y;

	i = -1;
	for ( row=0; row<32; ++row )
	{
		for ( col=0; col<32; ++col ) {
			if ( col % 8 ) {
				data[i] <<= 1;
				mask[i] <<= 1;
			} else {
				++i;
				data[i] = mask[i] = 0;
			}
	  
			switch (p_arrow[4+row][col]) {
			case 'X':
				data[i] |= 0x01;
				mask[i] |= 0x01;
				cursorImage[row][col] = 0;
				break;
			case '.':
				mask[i] |= 0x01;
				cursorImage[row][col] = 1;
				break;
			case ' ':
				cursorImage[row][col] = -1;
				break;
			}
		}
	}

	sscanf(p_arrow[4+row], "%d,%d", &hot_x, &hot_y);

	SDL_Cursor* p_cursor = SDL_CreateCursor(data, mask, 32, 32, hot_x, hot_y);
	SDL_SetCursor(p_cursor);
}