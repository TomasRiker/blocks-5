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
#include "sharpfit_shader.h"
#include "crt_shader.h"
#ifdef __EMSCRIPTEN__
#include "web_bluescreen.h"
#endif
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
	upscaleFilter = UF_SHARP_FIT;    // Voreinstellung; ohne Shader wird nearest daraus
	fullScreen = false;
	fullScreenOverride = -1;
	swallowedReturn = false;
	windowedSize = Vec2i(0, 0);      // 0 = noch nichts gewaehlt, init() entscheidet
	windowedPosition = Vec2i(0, 0);
	windowedPositionKnown = false;
	maximized = false;
#ifdef _WIN32
	inSizeMove = false;
#endif
	savedWindowStyle = 0;
	savedWindowRect[0] = savedWindowRect[1] = savedWindowRect[2] = savedWindowRect[3] = 0;
	sharpFit.program = 0;
	sharpFit.decal = sharpFit.textureSize = sharpFit.frameSize = sharpFit.prescale = -1;
	sharpFit.scanline = sharpFit.curvature = sharpFit.bloom = -1;
	sharpFit.flicker = sharpFit.time = sharpFit.scanPhase = sharpFit.scanFlicker = -1;
	crt = sharpFit;
	crtScanline = 0.5;
	crtCurvature = 0.5;
	crtBloom = 0.5;
	crtFlicker = 0.5;
	crtScanFlicker = 0.5;
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
	// Konfiguration laden. Setzt windowedSize, wenn die Datei etwas dazu sagt.
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

	// alle Tasten als VK einfuegen
	for(int k = 0; k < SDLK_LAST; k++)
	{
		VirtualKey vk;
		const char* p_name = SDL_GetKeyName(static_cast<SDLKey>(k));
		vk.name = std::string("Keyboard ") + (p_name ? p_name : "???");
		vk.key = k;
		vk.down = false;
		virtualKeys.push_back(vk);
	}

	// alle Joysticks oeffnen
	int n = SDL_NumJoysticks();
	int index = 0;
	for(int j = 0; j < n; j++)
	{
		SDL_Joystick* p_joystick = SDL_JoystickOpen(j);
		if(p_joystick)
		{
			// alle Tasten als VK einfuegen
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

			// alle Achsen als VK einfuegen
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

			// alle Hats mit allen Richtungen als VK einfuegen
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

	// Jetzt erst, denn getDesktopSize() braucht SDL - und es muss vor dem
	// ersten SDL_SetVideoMode passieren, weil SDL_GetVideoInfo danach die
	// Fenster- statt der Desktopgroesse liefert.
	if(windowedSize.x <= 0 || windowedSize.y <= 0) windowedSize = getDefaultWindowSize();
	// Ein Fenster, das nicht mehr auf den Bildschirm passt - anderer Rechner,
	// abgestoepselter Monitor -, wird zurechtgestutzt.
	const Vec2i desktop = getDesktopSize();
	if(windowedSize.x > desktop.x || windowedSize.y > desktop.y)
		windowedSize = getDefaultWindowSize();

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

	// Dorthin, wo es zuletzt stand.
	restoreWindowPosition();

#ifdef _WIN32
	// Ab jetzt steht das Fenster, und die eigene Fensterprozedur kann davor.
	hookWindowProc();
#endif

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

	// Bildpuffer anlegen. Schlaegt das fehl, rendert das Spiel wie frueher direkt
	// in den Backbuffer - dann ist nur die Fenstergroesse wieder starr.
	GLExtensions::init();
	useFrameBuffer = createFrameBuffer();
	if(!useFrameBuffer)
	{
		printfLog("- WARNING: No framebuffer object; rendering straight to the back buffer.\n");
	}
	else if(GLExtensions::haveShaders())
	{
		createPresentPrograms();
	}
	printfLog("  Upscale filters:  nearest, bilinear%s%s\n",
			  canUseSharpFit() ? ", sharp-fit" : "", canUseCrt() ? ", crt" : "");
	printfLog("  Upscaling:        %s\n", getUpscaleFilterName(getEffectiveUpscaleFilter()));

	// Texturen fuer Crossfading erzeugen
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

	// Ton fuer Videoaufnahmen: OpenAL kann nur Eingangsgeraete aufnehmen, also das
	// Mikrofon. Aufgenommen werden soll aber das, was das Spiel ausgibt - das macht
	// AudioCapture ueber den Loopback-Modus von WASAPI.
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

	// Fenstergroesse, -position und Vollbildzustand sollen den naechsten Start
	// erleben, auch wenn der Spieler den Optionsdialog nie geoeffnet hat - der
	// war bis hierher die einzige Stelle, die je gespeichert hat.
	rememberWindowPlacement();
	saveConfig();

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

#ifdef _WIN32
	// Eigene Fensterprozedur wieder heraus, bevor SDL das Fenster abbaut.
	unhookWindowProc();
#endif

	// Bildpuffer freigeben, solange der GL-Kontext noch steht
	destroyPresentPrograms();
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

	// Crossfade und Texturen loeschen
	crossfade(0, 0.0);
	glDeleteTextures(1, &oldImageID);
	glDeleteTextures(1, &newImageID);

	// Joysticks schliessen
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

	// Aktionen loeschen
	for(size_t i = 0; i < actionsVector.size(); i++) delete actionsVector[i];
	actionsVector.clear();
	actions.clear();

	saveTimePlayed();

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

							// Videoaufnahme stoppen, falls gerade eine laeuft
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
#ifndef __EMSCRIPTEN__
				// Alt+F4 muss das Spiel beenden. Windows macht daraus sonst
				// selbst ein WM_CLOSE - aber erst in DefWindowProc, und dorthin
				// kommt die Taste nie: SDLs windib-Fensterprozedur behandelt
				// WM_SYSKEYDOWN als gewoehnlichen Tastendruck und gibt 0
				// zurueck (SDL_dibevents.c:137). Also hier, und zwar genau so,
				// wie es der Quit-Knopf im Menue tut - damit auch eine laufende
				// Videoaufnahme sauber geschlossen wird.
				if(event.key.keysym.sym == SDLK_F4 &&
				   (event.key.keysym.mod & KMOD_ALT || SDL_GetModState() & KMOD_ALT))
				{
					SDL_Event quitEvent;
					quitEvent.type = SDL_QUIT;
					SDL_PushEvent(&quitEvent);
					break;
				}
#endif
				// Alt+Return schaltet Vollbild um und wird verschluckt, damit
				// das Spiel darin kein gewoehnliches Return sieht. (Im Browser
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
#ifdef __EMSCRIPTEN__
				// Im Browser kann sich ein Programm nicht selbst schliessen -
				// "Beenden" tat dort bisher schlicht gar nichts. Statt dessen
				// reisst das Spiel jetzt zum Schein den Rechner mit; siehe
				// WebBuild/web_bluescreen.cpp. Das haelt auch die Hauptschleife
				// an, done braucht es hier also nicht mehr.
				WebBlueScreen::show();
#else
				done = true;
#endif
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

			// Tastatur- und Mausdaten zuruecksetzen
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

			// Aktionsdaten zuruecksetzen
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
			// altes Bild sichern. Der Bildpuffer muss dafuer selbst gebunden
			// werden: gerendert wird nur, wenn dieser Durchgang mindestens
			// einen Logikschritt gemacht hat, und ohne das Rendern bleibt vom
			// Ende des vorigen Durchgangs der Bildschirm gebunden. Auf dem
			// nativen Weg kann das nicht vorkommen, weil das SDL_Delay unten
			// jeden Durchgang bis zum Logiktakt streckt - im Browser aber
			// dauernd, denn dort gibt der requestAnimationFrame den Takt vor
			// und ist mit 16,7 ms (oder 6,9 ms bei 144 Hz) schneller als die
			// 20 ms der Logik. Genau dann kopierte diese Zeile aus dem
			// Standard-Bildpuffer, und den leert WebGL vor jedem Bild - das
			// alte Bild war schwarz und blieb es die ganze Ueberblendung lang.
			bindFrameBuffer();
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

					// Bild holen. Immer 640x480 aus dem Bildpuffer, unabhaengig von der
					// Fenstergroesse - der Videoencoder ist einmal darauf eingerichtet.
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
			// festhaelt und nicht die skalierte Fassung samt schwarzer Balken.
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

	// Alle 30 Sekunden festhalten, auf beiden Plattformen. Im Browser ist es
	// die einzige Gelegenheit - exit() laeuft dort nie, weil
	// emscripten_set_main_loop nicht zurueckkehrt und ein Tab ohne Vorwarnung
	// geschlossen wird -, aber auch ein Absturz oder ein abgewuergter Prozess
	// unter Windows kostet so hoechstens eine halbe Minute statt der ganzen
	// Sitzung. Den Rest schreibt exit() genau nach.
	if(!(timePlayed % 1500)) saveTimePlayed();

#ifdef PROFILE_ENGINE_UPDATE
	END_PROFILE(engineUpdate)
#endif
}

// Getrennt, weil der Browser sie nebenher schreiben muss und nicht erst beim
// Beenden - dort kommt exit() nie an.
void Engine::saveTimePlayed()
{
	std::ostringstream timePlayedStr;
	timePlayedStr << timePlayed;
	FileSystem::inst().writeStringToFile(timePlayedStr.str(),
										 FileSystem::inst().getAppHomeDirectory() + ".time_played");
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

	// gestoppte Sounds loeschen
	for(std::list<StreamedSound*>::const_iterator i = toBeDeleted.begin(); i != toBeDeleted.end(); ++i) (*i)->release();

	if(volumeChanged) volumeChanged = false;
}

std::string Engine::getBestOpenALDevice()
{
	// Standardgeraet nehmen
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

bool Engine::createPresentProgram(PresentProgram& target, const char* p_fragmentSource,
								  const char* p_name)
{
	const uint vs = compileShaderStage(GL_VERTEX_SHADER, p_presentVertexShader, "present vertex");
	if(!vs) return false;
	const uint fs = compileShaderStage(GL_FRAGMENT_SHADER, p_fragmentSource, p_name);
	if(!fs) { glExtDeleteShader(vs); return false; }

	target.program = glExtCreateProgram();
	glExtAttachShader(target.program, vs);
	glExtAttachShader(target.program, fs);
	glExtBindAttribLocation(target.program, 0, "aPosition");
	glExtBindAttribLocation(target.program, 1, "aTexCoord");
	glExtLinkProgram(target.program);

	glExtDeleteShader(vs);
	glExtDeleteShader(fs);

	GLint ok = 0;
	glExtGetProgramiv(target.program, GL_LINK_STATUS, &ok);
	if(!ok)
	{
		char log[1024] = "";
		glExtGetProgramInfoLog(target.program, sizeof(log) - 1, 0, log);
		printfLog("- WARNING: Could not link the %s program: %s\n", p_name, log);
		destroyPresentProgram(target);
		return false;
	}

	target.decal       = glExtGetUniformLocation(target.program, "decal");
	target.textureSize = glExtGetUniformLocation(target.program, "TextureSize");
	target.frameSize   = glExtGetUniformLocation(target.program, "FrameSize");
	target.prescale    = glExtGetUniformLocation(target.program, "Prescale");
	// Die beiden gibt es nur im Roehrenshader; sonst bleibt es bei -1, und die
	// Uniform wird schlicht nicht gesetzt.
	target.scanline    = glExtGetUniformLocation(target.program, "Scanline");
	target.curvature   = glExtGetUniformLocation(target.program, "Curvature");
	target.bloom       = glExtGetUniformLocation(target.program, "Bloom");
	target.flicker     = glExtGetUniformLocation(target.program, "Flicker");
	target.time        = glExtGetUniformLocation(target.program, "Time");
	target.scanPhase   = glExtGetUniformLocation(target.program, "ScanPhase");
	target.scanFlicker = glExtGetUniformLocation(target.program, "ScanFlicker");
	return true;
}

void Engine::destroyPresentProgram(PresentProgram& target)
{
	if(target.program) { glExtDeleteProgram(target.program); target.program = 0; }
	target.decal = target.textureSize = target.frameSize = target.prescale = -1;
	target.scanline = target.curvature = target.bloom = target.flicker = -1;
	target.time = target.scanPhase = target.scanFlicker = -1;
}

bool Engine::createPresentPrograms()
{
	// WebGL verbietet Vertexdaten aus dem Anwendungsspeicher, es muss ein Puffer
	// sein. Vier Eckpunkte, jedes Bild neu gefuellt - das kostet nichts und
	// erspart es, auf Fenstergroessenaenderungen zu achten. Beide Programme
	// benutzen denselben.
	glExtGenBuffers(1, &presentVertexBuffer);
	if(!presentVertexBuffer)
	{
		printfLog("- WARNING: Could not create the present vertex buffer.\n");
		return false;
	}

	// Der Roehrenshader darf fehlschlagen, ohne dass sharp-fit mitfaellt.
	const bool sharpOk = createPresentProgram(sharpFit, p_sharpFitFragmentShader, "sharp-fit fragment");
	const bool crtOk   = createPresentProgram(crt, p_crtFragmentShader, "crt fragment");
	if(!crtOk) printfLog("- WARNING: The CRT filter will not be available.\n");

	if(!sharpOk)
	{
		destroyPresentProgram(crt);
		glExtDeleteBuffers(1, &presentVertexBuffer);
		presentVertexBuffer = 0;
		return false;
	}
	return true;
}

void Engine::destroyPresentPrograms()
{
	if(presentVertexBuffer) { glExtDeleteBuffers(1, &presentVertexBuffer); presentVertexBuffer = 0; }
	destroyPresentProgram(sharpFit);
	destroyPresentProgram(crt);
}

void Engine::setCrtScanline(double value)
{
	crtScanline = clamp(value, 0.0, 1.0);
}

void Engine::setCrtCurvature(double value)
{
	crtCurvature = clamp(value, 0.0, 1.0);
}

void Engine::setCrtBloom(double value)
{
	crtBloom = clamp(value, 0.0, 1.0);
}

void Engine::setCrtFlicker(double value)
{
	crtFlicker = clamp(value, 0.0, 1.0);
}

void Engine::setCrtScanFlicker(double value)
{
	crtScanFlicker = clamp(value, 0.0, 1.0);
}

void Engine::setUpscaleFilter(UpscaleFilter filter)
{
	// Nur merken. Ob der gewuenschte Filter wirklich geht, entscheidet
	// getEffectiveUpscaleFilter() bei jedem Bild neu - beim Laden der
	// config.xml gibt es noch keinen GL-Kontext, gegen den man pruefen koennte.
	upscaleFilter = filter;
}

const char* Engine::getUpscaleFilterName(UpscaleFilter filter)
{
	switch(filter)
	{
	case UF_NEAREST:    return "nearest";
	case UF_SHARP_FIT:  return "sharp-fit";
	case UF_CRT:        return "crt";
	default:            return "bilinear";
	}
}

Engine::UpscaleFilter Engine::parseUpscaleFilterName(const char* p_name, UpscaleFilter fallback)
{
	if(!p_name) return fallback;
	if(!_stricmp(p_name, "nearest"))     return UF_NEAREST;
	if(!_stricmp(p_name, "bilinear"))    return UF_BILINEAR;
	if(!_stricmp(p_name, "sharp-fit"))   return UF_SHARP_FIT;
	if(!_stricmp(p_name, "crt"))         return UF_CRT;
	return fallback;
}

bool Engine::canUseSharpFit() const
{
	return useFrameBuffer && sharpFit.program != 0 && presentVertexBuffer != 0;
}

bool Engine::canUseCrt() const
{
	return useFrameBuffer && crt.program != 0 && presentVertexBuffer != 0;
}

Engine::UpscaleFilter Engine::getEffectiveUpscaleFilter() const
{
	// Ohne uebersetztes Programm lieber scharf als ein Bild, das gar nicht erst
	// erscheint. Der Wunsch bleibt in upscaleFilter stehen, damit dieselbe
	// config.xml auf einer Maschine mit Shadern wieder das Richtige tut.
	if(upscaleFilter == UF_SHARP_FIT && !canUseSharpFit()) return UF_NEAREST;
	if(upscaleFilter == UF_CRT && !canUseCrt())            return UF_NEAREST;
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
	// einen Stencil-Puffer. Ein reiner Farbpuffer waere also zu wenig.
	glExtGenRenderbuffers(1, &frameDepthStencilID);
	glExtBindRenderbuffer(GL_RENDERBUFFER_EXT, frameDepthStencilID);
#ifdef __EMSCRIPTEN__
	// WebGL 1 kennt genau ein kombiniertes Format und einen kombinierten
	// Anhaengepunkt dafuer.
	glExtRenderbufferStorage(GL_RENDERBUFFER_EXT, GL_DEPTH_STENCIL_EXT,
							 frameTextureSize.x, frameTextureSize.y);
	glExtFramebufferRenderbuffer(GL_FRAMEBUFFER_EXT, GL_DEPTH_STENCIL_ATTACHMENT_EXT,
								 GL_RENDERBUFFER_EXT, frameDepthStencilID);
#else
	// EXT_packed_depth_stencil kennt keinen kombinierten Anhaengepunkt: derselbe
	// Renderbuffer wird an beide gehaengt, so schreibt es die Spezifikation vor.
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

Vec2i Engine::getDefaultWindowSize() const
{
	// Ganzzahlige Vielfache, damit auch "Scharf" von Anfang an ohne Balken
	// auskommt. Etwas Rand bleibt fuer Titelzeile und Taskleiste - sonst
	// klebt das Fenster an den Kanten oder passt gar nicht.
	//
	// Der Rand ist genau so bemessen, dass 1920x1080 - der haeufigste
	// Bildschirm ueberhaupt - noch die doppelte Groesse bekommt: 2*480 ist 960,
	// und 1080-120 ist auch 960, das geht gerade eben auf. Ein Pixel mehr Rand,
	// und es waere die einfache Groesse. Waagerecht derselbe Wert, obwohl dort
	// viel mehr Platz waere: die Taskleiste steht nicht ueberall unten.
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
	// Im Vollbild steht das Fenster auf (0,0) und ist bildschirmgross - das
	// waere die falsche Erinnerung. Dann zaehlt, was applyWindowStyle() sich
	// vor dem Umschalten gemerkt hat.
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

	// GetWindowPlacement statt GetWindowRect. Bei einem maximierten Fenster
	// liefert GetWindowRect den maximierten Rahmen - unter Windows sogar mit
	// negativen Ecken, weil die unsichtbaren Anfasser mitzaehlen -, und das
	// naechste Mal stand dann ein bildschirmgrosses Fenster an einer Stelle,
	// an der es zur Haelfte hinausragte. rcNormalPosition ist der Rahmen, auf
	// den "Wiederherstellen" zurueckgeht, und genau der gehoert gespeichert.
	WINDOWPLACEMENT wp;
	wp.length = sizeof(wp);
	if(!GetWindowPlacement(info.window, &wp)) return;

	maximized = (wp.showCmd == SW_SHOWMAXIMIZED);
	windowedPosition = Vec2i(wp.rcNormalPosition.left, wp.rcNormalPosition.top);
	windowedPositionKnown = true;

	// rcNormalPosition ist ein Fensterrahmen, windowedSize eine Nutzflaeche -
	// der Rahmen muss also abgezogen werden. Wie dick er ist, weiss nur
	// Windows; AdjustWindowRectEx auf ein leeres Rechteck gibt genau ihn.
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

	// Landet das Fenster auf keinem Bildschirm mehr - zweiter Monitor
	// abgezogen, Aufloesung kleiner geworden -, dann lieber dort lassen, wo
	// Windows es hingestellt hat. MonitorFromRect beantwortet das richtig,
	// auch fuer negative Koordinaten: ein Bildschirm links des ersten hat
	// welche, und die sind voellig in Ordnung.
	RECT r;
	r.left   = windowedPosition.x;
	r.top    = windowedPosition.y;
	r.right  = windowedPosition.x + displaySize.x;
	r.bottom = windowedPosition.y + displaySize.y;
	if(!MonitorFromRect(&r, MONITOR_DEFAULTTONULL)) return;

	SetWindowPos(info.window, HWND_NOTOPMOST, windowedPosition.x, windowedPosition.y,
				 0, 0, SWP_NOSIZE | SWP_NOZORDER);

	// Maximiert war es, maximiert kommt es wieder. SDL bekommt das ueber sein
	// eigenes WM_WINDOWPOSCHANGED mit und schickt ein SDL_VIDEORESIZE, das
	// handleResize() in der Hauptschleife aufgreift.
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
// Windows haelt die Anwendung an, solange der Benutzer den Fensterrand oder die
// Titelzeile festhaelt: WM_NCLBUTTONDOWN landet in DefWindowProc, und die dreht
// bis zum Loslassen eine eigene Nachrichtenschleife. Die Hauptschleife des
// Spiels steckt derweil in SDL_PollEvent fest, es wird nichts gezeichnet, und
// das Fenster zeigt beim Ziehen ein eingefrorenes oder leeres Bild.
//
// Herauskommt man da nur ueber die Fensterprozedur, denn die laeuft in dieser
// Schleife weiter. SDL 1.2 kennt WM_ENTERSIZEMOVE und WM_EXITSIZEMOVE nicht und
// reicht beides an DefWindowProc durch, also wird SDLs Prozedur eine eigene
// vorgeschaltet. Das ist derselbe Kniff, den SDL fuer SDL_WINDOWID selbst
// benutzt (DIB_CreateWindow in SDL_dibevents.c), und das Fenster entsteht genau
// einmal in DIB_VideoInit - ein weiteres SDL_SetVideoMode legt kein neues an,
// die Prozedur bleibt also haengen, wo sie ist.
static WNDPROC p_sdlWindowProc = 0;
static const UINT_PTR SIZEMOVE_TIMER_ID = 0xB5;

static LRESULT CALLBACK engineWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// Sollte nicht vorkommen - nach unhookWindowProc() ist diese Funktion nicht
	// mehr die Fensterprozedur -, aber ein Nullzeiger in CallWindowProc waere
	// ein Absturz beim Beenden, und das ist es nicht wert.
	if(!p_sdlWindowProc) return DefWindowProc(hwnd, msg, wParam, lParam);

	Engine& engine = Engine::inst();

	switch(msg)
	{
	case WM_ENTERSIZEMOVE:
		engine.setInSizeMove(true);
		// Der Zeitgeber ist fuer den Fall, dass der Benutzer den Rand
		// festhaelt, ohne ihn zu bewegen: dann kommt kein WM_SIZE mehr, und
		// ohne ihn stuende das Bild wieder. WM_TIMER hat niedrige Prioritaet
		// und wird waehrend einer Bewegung von den Mausnachrichten verdraengt -
		// genau dann zeichnet aber WM_SIZE.
		SetTimer(hwnd, SIZEMOVE_TIMER_ID, 15, 0);
		break;

	case WM_EXITSIZEMOVE:
		KillTimer(hwnd, SIZEMOVE_TIMER_ID);
		engine.setInSizeMove(false);
		// Aufgeraeumt wird nicht hier: SDL hat aus WM_WINDOWPOSCHANGED laengst
		// ein SDL_VIDEORESIZE gemacht, und handleResize() zieht die SDL-Seite
		// nach, sobald die Hauptschleife wieder laeuft.
		break;

	case WM_TIMER:
		if(wParam == SIZEMOVE_TIMER_ID)
		{
			engine.repaintDuringSizeMove();
			return 0;
		}
		break;

	case WM_SIZE:
		// Waehrend des Ziehens die eigentliche Quelle: kommt bei jedem Schritt.
		if(wParam != SIZE_MINIMIZED) engine.repaintDuringSizeMove();
		break;

	case WM_PAINT:
		// Waehrend eines Dateidialogs kommt WM_PAINT, sobald der Dialog das
		// Fenster freigibt oder darueber gezogen wird. Die Hauptschleife
		// steht dann; ohne diese Zeilen zeigt das Fenster ein altes Bild oder
		// gar keines. ValidateRect ist Pflicht: ein nicht abgeraeumtes
		// WM_PAINT kommt sofort wieder und dreht sich im Kreis.
		if(engine.isInSizeMove())
		{
			engine.repaintDuringSizeMove();
			ValidateRect(hwnd, 0);
			return 0;
		}
		break;

	case WM_GETMINMAXINFO:
		{
			// handleResize() klemmt ohnehin auf mindestens 640x480 hoch. Das
			// hier sagt es Windows vorher, damit der Rahmen schon beim Ziehen
			// stehenbleibt, statt hinterher zurueckzuspringen.
			//
			// Erst weiterreichen, dann aendern: die Struktur hat noch vier
			// weitere Felder - Groesse und Lage im maximierten Zustand und die
			// Obergrenze -, und die fuellt DefWindowProc. Wer hier einfach
			// selbst antwortet, laesst sie auf Verdacht stehen.
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
	if(fullScreen) return Vec2i(0, 0);   // im Vollbild zieht niemand am Rand

	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if(!SDL_GetWMInfo(&info) || !info.window) return Vec2i(0, 0);

	// Von der gewuenschten Nutzflaeche auf das Fensterrechteck: Rahmen und
	// Titelzeile kommen dazu, und wie dick die sind, weiss nur Windows.
	RECT r = { 0, 0, screenSize.x, screenSize.y };
	const LONG style   = GetWindowLong(info.window, GWL_STYLE);
	const LONG exStyle = GetWindowLong(info.window, GWL_EXSTYLE);
	if(!AdjustWindowRectEx(&r, style, FALSE, exStyle)) return Vec2i(0, 0);

	return Vec2i(r.right - r.left, r.bottom - r.top);
}

void Engine::flushInput()
{
	// Erst weg, was Windows waehrend des fremden Fensters aufgestaut hat -
	// aber nur Tasten und Maus. Alles andere muss stehenbleiben, allen voran
	// SDL_VIDEORESIZE: das ist die einzige Stelle, an der handleResize() von
	// einer neuen Fenstergroesse erfaehrt, und ein hier verschlucktes liesse
	// SDLs Oberflaeche fuer immer auf der alten stehen.
	SDL_Event events[32];
	SDL_PumpEvents();
	while(SDL_PeepEvents(events, 32, SDL_GETEVENT,
						 SDL_EVENTMASK(SDL_KEYDOWN) |
						 SDL_EVENTMASK(SDL_KEYUP) |
						 SDL_EVENTMASK(SDL_MOUSEBUTTONDOWN) |
						 SDL_EVENTMASK(SDL_MOUSEBUTTONUP) |
						 SDL_EVENTMASK(SDL_MOUSEMOTION)) > 0) {}

	// ... dann der eigene Zustand, samt der "gedrueckt"- und
	// "losgelassen"-Kennzeichen. Bliebe eine Maustaste als gedrueckt stehen,
	// laese die GUI das naechste Loslassen als Klick auf das Element unter dem
	// Zeiger - und das ist derselbe Knopf, der das Fenster geoeffnet hat.
	for(int i = 0; i < NUM_KEY_SLOTS; i++)
	{
		keyData[i] = 0;
		buttonData[i] = 0;
	}
	while(!keyEventQueue.empty()) keyEventQueue.pop();
}

void Engine::beginForeignMessageLoop()
{
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if(!SDL_GetWMInfo(&info) || !info.window) return;

	// Derselbe Zustand wie beim Ziehen am Fensterrand: eine fremde Schleife
	// pumpt die Nachrichten, die eigene laeuft nicht. Der Zeitgeber haelt das
	// Bild auch dann frisch, wenn gar kein WM_PAINT kommt.
	inSizeMove = true;
	SetTimer(info.window, SIZEMOVE_TIMER_ID, 15, 0);

	// Einmal sofort: der Dialog braucht einen Moment, bis er steht, und bis
	// dahin zeigt das Fenster sonst, was zufaellig darin liegt.
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
	// Nur waehrend der fremden Nachrichtenschleife. Ausserhalb zeichnet die
	// Hauptschleife, und die soll sich nichts dazwischenfunken lassen.
	if(!inSizeMove || !initialized || !useFrameBuffer) return;

	// SwapBuffers kann seinerseits Nachrichten zustellen; ein zweiter Durchlauf
	// mitten im ersten waere schlecht.
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

		// displaySize wird nur geliehen, nicht gesetzt. Bewusst ohne
		// SDL_SetVideoMode - das ruft SetWindowPos und wuerde dem Benutzer
		// waehrend des Ziehens ins Handwerk pfuschen; der GL-Kontext haengt am
		// Fenster und waechst von allein mit, presentFrame() braucht nur die
		// Zahl. Die SDL-Seite zieht handleResize() nach, sobald die
		// Hauptschleife wieder laeuft - und die erkennt eine Aenderung nur,
		// wenn displaySize hier so bleibt, wie SDL es kennt. Sonst faende sie
		// die neue Groesse gleich der alten, kaeme nie zu SDL_SetVideoMode, und
		// SDLs Oberflaeche bliebe fuer immer auf der alten Groesse stehen.
		if(w > 0 && h > 0)
		{
			const Vec2i knownToSDL = displaySize;

			displaySize = Vec2i(w, h);

			// Der Bildpuffer haelt das zuletzt gerenderte Bild noch - genau das
			// kommt jetzt in der neuen Groesse auf den Schirm, mit Balken und
			// Filter.
			unbindFrameBuffer();
			presentFrame();
			SDL_GL_SwapBuffers();

			displaySize = knownToSDL;
			unbindFrameBuffer();   // glViewport wieder passend zurueckstellen
		}
	}

	busy = false;
}
#endif

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
	// Maximiert nicht mitschreiben: sonst waere die gemerkte Fenstergroesse die
	// des maximierten Fensters, und "Wiederherstellen" haette beim naechsten
	// Start nichts mehr, worauf es zurueckgehen koennte.
	if(!fullScreen && !isWindowMaximized()) windowedSize = displaySize;
}

Vec2d Engine::warpToSource(const Vec2d& p) const
{
	// Genau die Formel aus src/crt_shader.h, damit die Maus dort landet, wo der
	// Spieler hinzeigt. p und der Rueckgabewert laufen von -1 bis 1 ab der
	// Bildmitte.
	if(getEffectiveUpscaleFilter() != UF_CRT || crtCurvature <= 0.0) return p;

	const double a = crtCurvature * crtCurveX;
	const double b = crtCurvature * crtCurveY;
	return Vec2d(p.x * (1.0 + a * p.y * p.y),
				 p.y * (1.0 + b * p.x * p.x));
}

Vec2d Engine::warpToOutput(const Vec2d& s) const
{
	if(getEffectiveUpscaleFilter() != UF_CRT || crtCurvature <= 0.0) return s;

	const double a = crtCurvature * crtCurveX;
	const double b = crtCurvature * crtCurveY;

	// Die Umkehrung. Das Gleichungspaar ist gekoppelt - x haengt an y und
	// umgekehrt -, eine geschlossene Loesung gibt es nicht. Als Fixpunkt
	// umgestellt zieht es sich aber sehr schnell zusammen:
	//
	//     x <- u / (1 + a*y^2)      y <- v / (1 + b*x^2)
	//
	// Nachgemessen ueber das ganze Bild: nach acht Runden liegt der Fehler
	// selbst bei einer voellig uebertriebenen Woelbung (a=0.40, b=0.50) unter
	// 2.3e-4 Bildpunkten, bei allem, was hier je eingestellt werden kann, unter
	// 1e-5. Das kostet nichts - die Funktion laeuft nur, wenn das Spiel den
	// Mauszeiger selbst setzt, und das tut es an genau einer Stelle.
	double x = s.x;
	double y = s.y;
	for(int i = 0; i < 8; i++)
	{
		x = s.x / (1.0 + a * y * y);
		y = s.y / (1.0 + b * x * x);
	}
	return Vec2d(x, y);
}

void Engine::computePresentRect(int& x, int& y, int& w, int& h) const
{
	// Groesstmoegliches 4:3-Rechteck im Fenster, mittig. Was uebrig bleibt, wird
	// schwarz - lieber Balken als ein verzerrtes Bild.
	double scale = min(static_cast<double>(displaySize.x) / screenSize.x,
					   static_cast<double>(displaySize.y) / screenSize.y);

	// "Scharf" braucht eine ganzzahlige Stufe. Bei einem krummen Faktor
	// verdoppelt Nearest manche Quellpixel und andere nicht - die Sprites
	// bekommen ungleiche Strichstaerken und die Schrift wird fransig, genau der
	// Fehler, den dieser Filter vermeiden soll. Lieber breitere Balken.
	// Unterhalb von 1:1 gibt es keine Stufe mehr, dann eben doch krumm: ein
	// abgeschnittenes Bild waere schlimmer.
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
	// braucht es keinen Shader.
	const UpscaleFilter effective = getEffectiveUpscaleFilter();
	// UF_SHARP_FIT rechnet die Texturkoordinate so um, dass die
	// Hardware-Interpolation genau das nearest-Ergebnis liefert - ohne sie
	// bliebe von dem Filter nichts uebrig. UF_CRT benutzt dieselbe Umrechnung
	// und braucht sie deshalb ebenso.
	const GLint filter = (effective == UF_BILINEAR || effective == UF_SHARP_FIT ||
						  effective == UF_CRT) ? GL_LINEAR : GL_NEAREST;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

	if(effective == UF_SHARP_FIT || effective == UF_CRT)
	{
		const PresentProgram& prog = (effective == UF_CRT) ? crt : sharpFit;

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

		// Der kleinste ganzzahlige Faktor, mit dem das 640x480-Bild das
		// Zielrechteck mindestens ausfuellt. Genau um den wuerde man mit
		// nearest vergroessern, bevor man auf die echte Groesse heruntergeht;
		// der Shader macht beides in einem Zug.
		const float prescaleX = static_cast<float>(max(1, static_cast<int>(ceil(static_cast<double>(w) / screenSize.x))));
		const float prescaleY = static_cast<float>(max(1, static_cast<int>(ceil(static_cast<double>(h) / screenSize.y))));

		glExtUseProgram(prog.program);
		if(prog.decal >= 0)       glExtUniform1i(prog.decal, 0);
		if(prog.textureSize >= 0) glExtUniform2f(prog.textureSize,
												 static_cast<float>(frameTextureSize.x),
												 static_cast<float>(frameTextureSize.y));
		if(prog.frameSize >= 0)   glExtUniform2f(prog.frameSize,
												 static_cast<float>(screenSize.x),
												 static_cast<float>(screenSize.y));
		if(prog.prescale >= 0)    glExtUniform2f(prog.prescale, prescaleX, prescaleY);
		// Die beiden Regler. Sie werden jedes Bild neu gesetzt, damit der
		// Optionsdialog sofort wirkt, ohne den Shader neu zu uebersetzen.
		if(prog.scanline >= 0)    glExtUniform1f(prog.scanline, static_cast<float>(crtScanline));
		if(prog.curvature >= 0)   glExtUniform1f(prog.curvature, static_cast<float>(crtCurvature));
		if(prog.bloom >= 0)       glExtUniform1f(prog.bloom, static_cast<float>(crtBloom));
		if(prog.flicker >= 0)     glExtUniform1f(prog.flicker, static_cast<float>(crtFlicker));
		if(prog.scanFlicker >= 0) glExtUniform1f(prog.scanFlicker, static_cast<float>(crtScanFlicker));
		// Die Wanduhr, nicht Engine::getTime() - die zaehlt in Logikschritten
		// und bleibt stehen, wenn das Spiel pausiert; ein Bildschirm flimmert
		// auch dann weiter. Der Umlauf ist genau FLICKER_CYCLE aus dem Shader,
		// und alle Frequenzen darin sind ganze Vielfache davon, also ist der
		// Sprung an der Nahtstelle keiner.
		const double seconds = static_cast<double>(SDL_GetTicks()) * 0.001;
		if(prog.time >= 0)
		{
			const double cycle = 8.0;   // = FLICKER_CYCLE in crt_shader.h
			glExtUniform1f(prog.time, static_cast<float>(fmod(seconds, cycle)));
		}
		// Das Zeilenkriechen. Als einziger Anteil des Flimmerns wird es hier
		// gerechnet und nicht im Shader: es ist eine Rampe, keine Schwingung,
		// und ihre Steigung haengt am Regler. Nimmt man dafuer die im Shader
		// schon gekuerzte Uhr, springt die Phase bei jedem Umlauf um
		// fract(Flimmern * Geschwindigkeit) einer Zeilenperiode. Aus der
		// ungekuerzten Uhr modulo 1 kommt sie stetig heraus.
		if(prog.scanPhase >= 0)
		{
			glExtUniform1f(prog.scanPhase,
						   static_cast<float>(fmod(seconds * crtCrawlSpeed * crtScanFlicker, 1.0)));
		}

		glExtBindBuffer(GL_ARRAY_BUFFER, presentVertexBuffer);
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
	// Immer das interne 640x480-Bild, nie die skalierte Fassung: der gewaehlte
	// Filter ist eine Anzeigeeinstellung und gehoert nicht in die Datei, und
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
			// Hoehe setzen
			if(pitchSpectrum != 0.0) p_inst->setPitch(1.0 + random(-pitchSpectrum, pitchSpectrum));

			// Prioritaet setzen
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

	// alle Zustaende verlassen
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

		// neuer Zustand erhaelt den Fokus
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
			// gedrueckt
			if(!a.countDown)
			{
				a.data |= 2;
				a.countDown = a.delay;

				// entgegengesetzte Aktionen zuruecksetzen
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
			// gedrueckt und vorher auch gedrueckt
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

					// entgegengesetzte Aktionen zuruecksetzen
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
		// liegt mittig, deshalb ist der Rand oben so gross wie unten und die
		// Rechnung gilt gleichermassen in SDLs Fensterkoordinaten (y nach unten)
		// wie in GLs (y nach oben).
		//
		// Gerechnet wird mit Pixelmitten: ein Fensterpixel px deckt [px, px+1)
		// ab, seine Mitte liegt bei px+0.5. Frueher stand hier die linke Kante
		// und ein static_cast<int>, was am krummen Vergroesserungsfaktor
		// regelmaessig einen Bildpunkt zu wenig ergab - nachgemessen: mit
		// Pixelmitten ist der Hin- und Rueckweg ueber jeden Bildpunkt exakt.
		int x, y, w, h;
		computePresentRect(x, y, w, h);
		if(w > 0 && h > 0)
		{
			Vec2d n((position.x + 0.5 - x) / w, (position.y + 0.5 - y) / h);

			// Dieselbe Woelbung wie im Shader: der Zeiger sitzt auf dem Glas,
			// das Spiel muss wissen, welches Quellpixel darunter liegt. Ohne
			// UF_CRT gibt warpToSource die Koordinate unveraendert zurueck.
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

void Engine::setCursorPosition(const Vec2i& cursorPosition)
{
	// Erst in den gueltigen Bereich des internen Bildes, dann nach aussen
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
			Vec2d n((temp.x + 0.5) / screenSize.x, (temp.y + 0.5) / screenSize.y);

			// Der Rueckweg durch die Woelbung. Bei ausgeschaltetem CRT-Filter
			// ist das die Identitaet.
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
		// altes Bild sichern - wie oben aus dem Bildpuffer, nicht aus dem,
		// was gerade gebunden ist.
		bindFrameBuffer();
		glBindTexture(GL_TEXTURE_2D, oldImageID);
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, screenPow2Size.y - screenSize.y, 0, 0, screenSize.x, screenSize.y);
		crossfadeTime = -0.5;
	}
}

std::string Engine::detectSystemLanguage()
{
	// Nur "de" oder "en". Von den 349 Zeichenketten in data/languages.txt haben
	// genau eine einen franzoesischen und eine einen spanischen Text - das sind
	// Platzhalter, keine Uebersetzungen. Wer hier "fr" erkennen wuerde, bekaeme
	// ein englisches Spiel mit franzoesischer Beschriftung. Alles ausser Deutsch
	// ist deshalb absichtlich Englisch.
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
	// Die Sprache der Oberflaeche, nicht das Gebietsschema: wer sein Windows
	// auf Deutsch benutzt, will das Spiel auf Deutsch, auch wenn Zahlen und
	// Datum anders eingestellt sind. GetLocaleInfoA, nicht ...W - das Projekt
	// ist MultiByte, siehe CLAUDE.md.
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
	// Ohne <Language> in der config.xml entscheidet das System. Das ist der
	// erste Start ohne Installer, der Browser, und genau der Fall, in dem
	// frueher stumm Englisch herauskam.
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
		// Sprache lesen
		TiXmlElement* p_language = p_config->FirstChildElement("Language");
		if(p_language)
		{
			const char* p_text = p_language->GetText();
			if(p_text) setLanguage(p_text);
		}
		else printfLog("  No <Language> in config.xml; using the system language: %s\n", language.c_str());

		// Skalierungsfilter lesen. Steht nichts da, bleibt die Voreinstellung.
		// Ob xBR wirklich geht, entscheidet getEffectiveUpscaleFilter() spaeter -
		// hier gibt es noch keinen GL-Kontext.
		TiXmlElement* p_upscaler = p_config->FirstChildElement("Upscaler");
		if(p_upscaler) upscaleFilter = parseUpscaleFilterName(p_upscaler->GetText(), upscaleFilter);

		// Die beiden Regler des Roehrenfilters. Fehlen sie, bleibt es bei der
		// Voreinstellung aus dem Konstruktor.
		TiXmlElement* p_crt = p_config->FirstChildElement("Crt");
		if(p_crt)
		{
			double value = 0.0;
			if(p_crt->QueryDoubleAttribute("scanline", &value) == TIXML_SUCCESS)
				setCrtScanline(value);
			if(p_crt->QueryDoubleAttribute("curvature", &value) == TIXML_SUCCESS)
				setCrtCurvature(value);
			if(p_crt->QueryDoubleAttribute("bloom", &value) == TIXML_SUCCESS)
				setCrtBloom(value);
			if(p_crt->QueryDoubleAttribute("flicker", &value) == TIXML_SUCCESS)
				setCrtFlicker(value);
			if(p_crt->QueryDoubleAttribute("scanflicker", &value) == TIXML_SUCCESS)
				setCrtScanFlicker(value);
		}

		// Vollbild und Fenstergroesse lesen. Beide gelten erst beim naechsten
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

		TiXmlElement* p_windowPosition = p_config->FirstChildElement("WindowPosition");
		if(p_windowPosition)
		{
			// Negative Werte sind erlaubt: ein zweiter Bildschirm links des
			// ersten hat sie. Ob die Stelle noch existiert, entscheidet
			// restoreWindowPosition() mit MonitorFromRect.
			int x = 0, y = 0;
			const bool haveX = p_windowPosition->QueryIntAttribute("x", &x) == TIXML_SUCCESS;
			const bool haveY = p_windowPosition->QueryIntAttribute("y", &y) == TIXML_SUCCESS;
			if(haveX && haveY && abs(x) <= 32768 && abs(y) <= 32768)
			{
				windowedPosition = Vec2i(x, y);
				windowedPositionKnown = true;
			}

			int max = 0;
			p_windowPosition->QueryIntAttribute("maximized", &max);
			maximized = (max != 0);
		}

		TiXmlElement* p_windowSize = p_config->FirstChildElement("WindowSize");
		if(p_windowSize)
		{
			int w = 0, h = 0;
			p_windowSize->QueryIntAttribute("w", &w);
			p_windowSize->QueryIntAttribute("h", &h);
			// Kleiner als das interne Bild ergibt keinen Sinn, und eine
			// unsinnig grosse Zahl aus einer verbogenen Datei auch nicht.
			if(w >= screenSize.x && h >= screenSize.y && w <= 16384 && h <= 16384)
				windowedSize = Vec2i(w, h);
		}

		// Sound-Lautstaerke lesen
		TiXmlElement* p_soundVolume = p_config->FirstChildElement("SoundVolume");
		if(p_soundVolume)
		{
			const char* p_text = p_soundVolume->GetText();
			if(p_text) setSoundVolume(atof(p_text));
		}

		// Musik-Lautstaerke lesen
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

	// Vollbild und Fenstergroesse schreiben, damit das Spiel so wiederkommt,
	// wie es verlassen wurde.
	TiXmlElement* p_fullScreen = new TiXmlElement("Fullscreen");
	p_fullScreen->LinkEndChild(new TiXmlText(fullScreen ? "1" : "0"));
	p_config->LinkEndChild(p_fullScreen);

	TiXmlElement* p_windowSize = new TiXmlElement("WindowSize");
	p_windowSize->SetAttribute("w", windowedSize.x);
	p_windowSize->SetAttribute("h", windowedSize.y);
	p_config->LinkEndChild(p_windowSize);

	TiXmlElement* p_crt = new TiXmlElement("Crt");
	p_crt->SetDoubleAttribute("scanline", crtScanline);
	p_crt->SetDoubleAttribute("curvature", crtCurvature);
	p_crt->SetDoubleAttribute("bloom", crtBloom);
	p_crt->SetDoubleAttribute("flicker", crtFlicker);
	p_crt->SetDoubleAttribute("scanflicker", crtScanFlicker);
	p_config->LinkEndChild(p_crt);

	if(windowedPositionKnown)
	{
		TiXmlElement* p_windowPosition = new TiXmlElement("WindowPosition");
		p_windowPosition->SetAttribute("x", windowedPosition.x);
		p_windowPosition->SetAttribute("y", windowedPosition.y);
		p_windowPosition->SetAttribute("maximized", maximized ? 1 : 0);
		p_config->LinkEndChild(p_windowPosition);
	}

	// Sound-Lautstaerke schreiben
	TiXmlElement* p_soundVolume = new TiXmlElement("SoundVolume");
	char temp[256] = "";
	sprintf(temp, "%f", getSoundVolume());
	p_soundVolume->LinkEndChild(new TiXmlText(temp));
	p_config->LinkEndChild(p_soundVolume);

	// Musik-Lautstaerke schreiben
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

			// \n nach \r ueberspringen
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
	const std::string patternStart = std::string("\xA7") + language + std::string(":");

	std::string::size_type indexStart = text.find(patternStart);
	if(std::string::npos == indexStart)
	{
		// Keine Lokalisierung fuer diese Sprache!
		if(language == "en")
		{
			// String unveraendert liefern
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

	std::string::size_type textEnd = text.find("\xA7", textStart);
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
	// Der Pfeil, doppelt so gross wie frueher. Seit das Fenster beliebig gross
	// sein darf, wird das 640x480-Bild mitskaliert - der Mauszeiger aber nicht,
	// den zeichnet das System in Fensterpixeln, und danebengehalten wirkt er
	// winzig. Schlicht pixelverdoppelt, jedes 1x1 wird ein 2x2: derselbe Pfeil
	// wie eh und je, nur groesser, mit allem was dazugehoert - der Umriss ist
	// zwei Pixel breit und die Schraege eine 2x2-Treppe. Groesser als 32x32
	// geht nicht, so gross ist ein Win32-Cursor.
	const char* p_arrow[] = {
		/* width height num_colors chars_per_pixel */
		"    32    32        3            1",
		/* colors */
		"X c #000000",
		". c #ffffff",
		"  c None",
		/* pixels */
		"XX                              ",
		"XX                              ",
		"XXXX                            ",
		"XXXX                            ",
		"XX..XX                          ",
		"XX..XX                          ",
		"XX....XX                        ",
		"XX....XX                        ",
		"XX......XX                      ",
		"XX......XX                      ",
		"XX........XX                    ",
		"XX........XX                    ",
		"XX..........XX                  ",
		"XX..........XX                  ",
		"XX............XX                ",
		"XX............XX                ",
		"XX..............XX              ",
		"XX..............XX              ",
		"XX..........XXXXXX              ",
		"XX..........XXXXXX              ",
		"XX....XX....XX                  ",
		"XX....XX....XX                  ",
		"XX..XX  XX....XX                ",
		"XX..XX  XX....XX                ",
		"XXXX    XX....XX                ",
		"XXXX    XX....XX                ",
		"          XX....XX              ",
		"          XX....XX              ",
		"          XX....XX              ",
		"          XX....XX              ",
		"            XXXX                ",
		"            XXXX                ",
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