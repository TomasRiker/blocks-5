#include "pch.h"
#include "engine.h"
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
#include "hq2x.h"
#include "videorecorder.h"

Engine::Engine()
{
	initialized = false;

	for(int i = 0; i < 512; i++)
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
	p_hq2xIn = 0;
	p_hq2xOut = 0;
	p_videoRecorder = 0;
	p_muteIconTexture = 0;
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
				  bool fullScreen,
				  bool useHQ2X)
{
	if(initialized) return false;

	// Konfiguration laden
	loadConfig();

	printfLog("* Language: %s\n", language.c_str());

	screenSize = Vec2i(width, height);
	screenPow2Size = Vec2i(nextPow2(width), nextPow2(height));
	this->fullScreen = fullScreen;
	this->useHQ2X = useHQ2X;

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

	if(useHQ2X)
	{
		// HQ2X initialisieren
		if(!InitLUTs())
		{
			// Kein MMX - dann können wir es sowieso vergessen.
			useHQ2X = false;
		}
		else
		{
			p_hq2xIn = new unsigned char[screenSize.x * screenSize.y * 4];
			p_hq2xOut = new unsigned char[screenSize.x * screenSize.y * 16];
		}
	}

	displaySize = Vec2i(width, height);
	if(useHQ2X)
	{
		displaySize.x = width * 2;
		displaySize.y = height * 2;

		if(fullScreen)
		{
			// die nächste passende Auflösung suchen
			SDL_Rect** pp_modes = SDL_ListModes(0, SDL_OPENGL | SDL_FULLSCREEN);
			if(pp_modes[0] == reinterpret_cast<SDL_Rect*>(-1))
			{
				// Keine Einschränkungen!
			}
			else
			{
				int i;
				for(i = 0; pp_modes[i]; i++);
				for(i--; i >= 0; i--)
				{
					// Passt das?
					if(pp_modes[i]->w >= displaySize.x && pp_modes[i]->h >= displaySize.y)
					{
						// Ja.
						displaySize.x = pp_modes[i]->w;
						displaySize.y = pp_modes[i]->h;
						break;
					}
				}
			}
		}
	}

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

	p_display = SDL_SetVideoMode(displaySize.x, displaySize.y, 32, SDL_OPENGL | (fullScreen ? SDL_FULLSCREEN : 0));
	if(!p_display)
	{
		printfLog("+ ERROR: Could not set video mode (Error: %s).\n", SDL_GetError());
		return false;
	}

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
	printfLog("  hq2x:             %s\n", useHQ2X ? "On" : "Off");
	printfLog("  ============================================================\n");

	// Extensions abfragen
	const char* p_extensions = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
	if(strstr(p_extensions, "GL_EXT_blend_func_separate"))
	{
		void* p_proc = SDL_GL_GetProcAddress("glBlendFuncSeparate");
		if(p_proc)
		{
			printfLog("  Extension GL_EXT_blend_func_separate is available.\n");
			printfLog("  ============================================================\n");
			glExtBlendFuncSeparate = static_cast<PFNGLBLENDFUNCSEPARATEEXTPROC>(p_proc);
		}
	}

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
	std::string bestCaptureDevice = getBestOpenALCaptureDevice();
	if(bestDevice == "[NONE]" || bestCaptureDevice == "[NONE]")
	{
		printfLog("+ ERROR: Please install current version of OpenAL and audio drivers.\n");
		return false;
	}

	printfLog("  ============================================================\n");
	printfLog("  Selected output:  %s\n", bestDevice.c_str());
	printfLog("  Selected capture: %s\n", bestCaptureDevice.c_str());
	printfLog("  ============================================================\n");

	p_audioDevice = alcOpenDevice(bestDevice.c_str());
	if(!p_audioDevice)
	{
		printfLog("+ ERROR: Could not open audio device.\n");
		return false;
	}

	p_audioCaptureDevice = alcCaptureOpenDevice(bestCaptureDevice.c_str(), 48000, AL_FORMAT_STEREO16, 48000);
	if(!p_audioCaptureDevice) printfLog("+ WARNING: Could not open audio capture device. Captured videos will be without audio!\n");

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

	av_register_all();

	printfLog("* Initializing GUI ...\n");
	if(!GUI::inst().init())
	{
		printfLog("+ ERROR: Could not initialize GUI.\n");
		return false;
	}

	// OpenGL-Einstellungen setzen
	glViewport(0, 0, width, height);
	glShadeModel(GL_SMOOTH);
	glEnable(GL_BLEND);
	glEnable(GL_POINT_SMOOTH);
	glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

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
	if(p_audioCaptureDevice) alcCaptureCloseDevice(p_audioCaptureDevice);

	// Crossfade und Texturen löschen
	crossfade(0, 0.0);
	glDeleteTextures(1, &oldImageID);
	glDeleteTextures(1, &newImageID);

	if(useHQ2X)
	{
		// hq2x herunterfahren
		delete[] p_hq2xIn;
		delete[] p_hq2xOut;
		p_hq2xIn = 0;
		p_hq2xOut = 0;
	}

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

void Engine::mainLoop()
{
	bool active = true;
	bool done = false;
	Uint32 timeToProcess = 0;
	uint timeProcessed = 1;
	uint firstEventRecorded = ~0;

	// Cursor-Position abfragen
	SDL_GetMouseState(&cursorPosition.x, &cursorPosition.y);

	processGameStateChanges();

#ifdef RECORD
	FILE* p_out = fopen("keyboard.dat", "wb");
#endif

	do
	{
		Uint32 start = SDL_GetTicks();

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
				keyData[event.key.keysym.sym] |= (1 | 2);
				keyEventQueue.push(event.key);
				break;
			case SDL_KEYUP:
				keyData[event.key.keysym.sym] &= ~1;
				keyData[event.key.keysym.sym] |= 4;
				keyEventQueue.push(event.key);
				break;
			case SDL_MOUSEBUTTONDOWN:
				buttonData[event.button.button] |= (1 | 2);
				break;
			case SDL_MOUSEBUTTONUP:
				buttonData[event.button.button] &= ~1;
				buttonData[event.button.button] |= 4;
				break;
			case SDL_MOUSEMOTION:
				cursorPosition = Vec2i(event.motion.x, event.motion.y);
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
			for(int i = 0; i < 512; i++)
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
			for(int i = 0; i < 512; i++)
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
			for(stdext::hash_map<std::string, Action*>::const_iterator it = actions.begin();
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

					// Bild holen
					glReadBuffer(GL_BACK);
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
			upscaleFrame();

			if(doScreenshot)
			{
				doScreenshot = false;
				screenshot();
				playSound("screenshot.ogg");
			}

			// gerendertes Frame anzeigen
			SDL_GL_SwapBuffers();
		}

		Uint32 end = SDL_GetTicks();
		if(frameRendered) frameTime = end - start;

		// warten, wenn noch genug Zeit ist
		uint dt = end - start;
		if(timeToProcess + dt < logicRate)
		{
			SDL_Delay(logicRate - (timeToProcess + dt));
			end = SDL_GetTicks();
			dt = end - start;
		}

		timeToProcess += dt;
		timeToProcess = min<uint>(250, timeToProcess);

	} while(!done);

#ifdef RECORD
	fclose(p_out);
#endif
}

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
			const std::string filename(FileSystem::inst().getAppHomeDirectory() + "videos/" + videoDateTime + ".avi");

			// Aufnahme starten
			p_videoRecorder = new VideoRecorder(filename, screenSize, screenSize, 2840000, p_audioCaptureDevice ? 160000 : 0, 30);
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
		pushGameState(s[random() % 3]);
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
	const stdext::hash_multimap<std::string, Sound*>& sounds = Manager<Sound>::inst().getItems();
	for(stdext::hash_multimap<std::string, Sound*>::const_iterator i = sounds.begin(); i != sounds.end(); ++i) i->second->update();

	// gestreamte Sounds aktualisieren
	const stdext::hash_multimap<std::string, StreamedSound*>& streamedSounds = Manager<StreamedSound>::inst().getItems();
	std::list<StreamedSound*> toBeDeleted;
	for(stdext::hash_multimap<std::string, StreamedSound*>::const_iterator i = streamedSounds.begin(); i != streamedSounds.end(); ++i)
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

std::string Engine::getBestOpenALCaptureDevice()
{
	// Standardgerät nehmen
	const char* p_device = alcGetString(0, ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER);
	if(!p_device) return "[NONE]";
	else return p_device;
}

// #define PROFILE_HQ2X

void Engine::upscaleFrame()
{
	if(!useHQ2X) return;

	// Code für HQ2X

	glPushAttrib(GL_ALL_ATTRIB_BITS);

	glReadBuffer(GL_BACK);
	glDrawBuffer(GL_BACK);

	glDisable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	gluOrtho2D(0.0, displaySize.x, 0.0, displaySize.y);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

#ifdef PROFILE_HQ2X
	BEGIN_PROFILE(HQ2X)
#endif

	glReadPixels(0, 0, screenSize.x, screenSize.y, GL_RGBA, GL_UNSIGNED_BYTE, p_hq2xIn);
	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);

	// in 16-Bit umwandeln
	uint* p_in = reinterpret_cast<uint*>(p_hq2xIn);
	uint* const p_in_end = p_in + screenSize.x * screenSize.y;
	ushort* p_out = reinterpret_cast<ushort*>(p_hq2xIn);
	for(; p_in != p_in_end; p_in++, p_out++)
	{
		const uint r = (((*p_in) & 0x00FF0000) >> (16 + 3)) << 11;
		const uint g = (((*p_in) & 0x0000FF00) >> (8 + 2)) << 5;
		const uint b = ((*p_in) & 0x000000FF) >> (0 + 3);
		*p_out = r | g | b;
	}

	hq2x_32(p_hq2xIn, p_hq2xOut, screenSize.x, screenSize.y, screenSize.x * 8);

	glRasterPos2i(0, displaySize.y - screenSize.y * 2);
	glDrawPixels(screenSize.x * 2, screenSize.y * 2, GL_RGBA, GL_UNSIGNED_BYTE, p_hq2xOut);

#ifdef PROFILE_HQ2X
	END_PROFILE(HQ2X)
#endif
	
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();

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
	char* p_temp = new char[displaySize.x * displaySize.y * 3];

	glReadBuffer(GL_BACK);
	glReadPixels(0, 0, displaySize.x, displaySize.y, GL_BGR, GL_UNSIGNED_BYTE, p_temp);

	// Zeilen richtigherum drehen
	char* p_buffer = new char[displaySize.x * displaySize.y * 3];
	char* p_cursor = p_temp;
	for(int y = 0; y < displaySize.y; y++)
	{
		int ny = displaySize.y - 1 - y;
		memcpy(p_buffer + displaySize.x * 3 * ny, p_cursor, displaySize.x * 3);
		p_cursor += displaySize.x * 3;
	}

	delete[] p_temp;

	SDL_Surface* p_surface = SDL_CreateRGBSurfaceFrom(p_buffer, displaySize.x, displaySize.y, 24, displaySize.x * 3, 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00000000);

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
	stdext::hash_map<std::string, GameState*>::const_iterator i = gameStates.find(gs);
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
	return keyData[key] & 1 ? true : false;
}

bool Engine::wasKeyPressed(SDLKey key) const
{
	return keyData[key] & 2 ? true : false;
}

bool Engine::wasKeyReleased(SDLKey key) const
{
	return keyData[key] & 4 ? true : false;
}

void Engine::setKeyDown(SDLKey key,
						bool status)
{
	if(status) keyData[key] |= 1;
	else keyData[key] &= ~1;
}

void Engine::setKeyPressed(SDLKey key,
						   bool status)
{
	if(status) keyData[key] |= 2;
	else keyData[key] &= ~2;
}

void Engine::setKeyReleased(SDLKey key,
							bool status)
{
	if(status) keyData[key] |= 4;
	else keyData[key] &= ~4;
}

void Engine::setKeyData(SDLKey key,
						int data)
{
	keyData[key] = data;
}

bool Engine::isButtonDown(uint button) const
{
	return buttonData[button] & 1 ? true : false;
}

bool Engine::wasButtonPressed(uint button) const
{
	return buttonData[button] & 2 ? true : false;
}

bool Engine::wasButtonReleased(uint button) const
{
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

const stdext::hash_map<std::string, Action*>& Engine::getActions() const
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
	stdext::hash_map<std::string, Action*>::const_iterator it = actions.find(name);
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
	Uint8* p_keys = SDL_GetKeyState(0);
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
	for(stdext::hash_map<std::string, Action*>::const_iterator it = actions.begin();
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
	for(stdext::hash_map<std::string, Action*>::const_iterator it = actions.begin();
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

	if(useHQ2X)
	{
		int offset = (displaySize.y - screenSize.y * 2) / 2;
		position.y -= offset;
		position /= 2;
	}

	position = Vec2i(clamp(position.x, 0, screenSize.x - 1),
					 clamp(position.y, 0, screenSize.y - 1));

	return position;
}

void Engine::setCursorPosition(const Vec2i& cursorPosition)
{
	Vec2i temp = cursorPosition;

	if(useHQ2X)
	{
		temp = Vec2i(clamp(temp.x, 0, temp.x - 1),
					 clamp(temp.y, 0, temp.y - 1));
		temp *= 2;
		int offset = (displaySize.y - screenSize.y * 2) / 2;
		temp.y += offset;
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
			stdext::hash_map<std::string, std::string>::const_iterator i = stringDB.find(text);
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
	stdext::hash_map<std::string, std::string>::const_iterator i = stringDB.find(id);
	if(i == stringDB.end()) return id;
	else return i->second;
}

ALCdevice* Engine::getOpenALCaptureDevice()
{
	return p_audioCaptureDevice;
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