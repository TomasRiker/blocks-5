#ifndef _ENGINE_H
#define _ENGINE_H

/*** Klasse der Engine ***/

#include "parameterblock.h"

class GameState;
class SoundInstance;
class StreamedSound;
class Texture;
class Crossfade;
class VideoRecorder;
class AudioCapture;

struct Action
{
	std::string name;
	int primary;
	int secondary;
	int delay;
	int interval;
	int defaultPrimary;
	int defaultSecondary;
	std::vector<std::string> resetsActions;

	int data;
	int countDown;
	int buffered;
};

struct VirtualKey
{
	std::string name;
	int device;
	int key;
	int axis;
	bool positive;
	int hat;
	int hatDir;

	bool down;

	VirtualKey()
		: device(-1)
		, key(-1)
		, axis(-1)
		, hat(-1)
	{
	}
};

class Engine : public Singleton<Engine>
{
	friend class Singleton<Engine>;

public:
	// Wie das intern gerenderte 640x480-Bild auf den Bildschirm kommt.
	enum UpscaleFilter
	{
		UF_NEAREST = 0,   // harte Kanten; nur bei ganzzahliger Vergroesserung sinnvoll
		UF_BILINEAR,      // die Hardware macht es, kostet nichts
		UF_SHARP_FIT,     // nearest auf die naechste ganzzahlige Stufe, dann herunter
		UF_XBR,           // kantengefuehrt, siehe libs/xbr
		UF_XBR_DETAIL     // dito mit small_details=1: glaettet auch Raster wie das Gras
	};
	bool init(const std::string& windowCaption, const std::string& windowIconFilename, uint width, uint height, bool defaultFullScreen);
	void exit();
	void mainLoop();
#ifdef __EMSCRIPTEN__
	void mainLoopIteration();   // one frame, driven by the browser
#endif
	void render();
	void update();
	void updateSounds();

	std::string getBestOpenALDevice();
	void drawOverlays();
	void screenshot();

	// Der Bildpuffer, in den das Spiel rendert. Immer 640x480, unabhaengig
	// davon, wie gross das Fenster ist; presentFrame() bringt ihn danach auf
	// den Bildschirm. Alles, was in Bildschirmkoordinaten rechnet - der eine
	// glViewport, die glScissor-Aufrufe, die GUI-Layouts - bleibt dadurch
	// gueltig, egal wie das Fenster skaliert wird.
	bool createFrameBuffer();
	void destroyFrameBuffer();
	void bindFrameBuffer();      // Ziel = Bildpuffer, Viewport 640x480
	void unbindFrameBuffer();    // Ziel = Fenster
	void presentFrame();         // Bildpuffer -> Fenster, mit schwarzen Balken
	// Wohin im Fenster das 640x480-Bild kommt: mittig, Seitenverhaeltnis
	// erhalten. Auch die Umkehrung fuer die Mausposition benutzt genau das.
	void computePresentRect(int& x, int& y, int& w, int& h) const;

	// Das Fenster. Es ist immer in der Groesse veraenderbar; Vollbild ist nur
	// eine besondere Groesse plus ein Stilwechsel am Win32-Fenster vorbei an
	// SDL, damit der GL-Kontext dabei am Leben bleibt. Deshalb bleiben SDLs
	// Flags das ganze Programm ueber genau SDL_OPENGL | SDL_RESIZABLE.
	//
	// overrideFullScreen() gehoert vor init(): -windowed / -fullscreen schlagen
	// damit, was in der config.xml steht.
	void overrideFullScreen(bool wantFullScreen) { fullScreenOverride = wantFullScreen ? 1 : 0; }
	void handleResize(int width, int height);   // auf SDL_VIDEORESIZE hin
	void setFullScreen(bool wantFullScreen);
	void toggleFullScreen() { setFullScreen(!fullScreen); }
	bool isFullScreen() const { return fullScreen; }
	Vec2i getDesktopSize() const;

	// Der xBR-Filter. Laesst er sich nicht uebersetzen, faellt die Anzeige auf
	// UF_BILINEAR zurueck; das Spiel laeuft in jedem Fall.
	bool createXbrProgram();
	void destroyXbrProgram();
	// Und der billige Verwandte: ein Fetch, siehe src/sharpfit_shader.h.
	bool createSharpFitProgram();
	void destroySharpFitProgram();

	// getUpscaleFilter() liefert den *Wunsch* - das, was der Spieler gewählt
	// hat und was in der config.xml steht. getEffectiveUpscaleFilter() liefert,
	// was tatsächlich gezeichnet wird: ohne übersetztes Programm wird aus xBR
	// bilinear. Der Wunsch bleibt dabei stehen, damit dieselbe config.xml auf
	// einer Maschine mit Shadern wieder das Richtige tut.
	void setUpscaleFilter(UpscaleFilter filter);
	UpscaleFilter getUpscaleFilter() const { return upscaleFilter; }
	UpscaleFilter getEffectiveUpscaleFilter() const;
	// Die Namen, unter denen der Filter in der config.xml steht.
	static const char* getUpscaleFilterName(UpscaleFilter filter);
	static UpscaleFilter parseUpscaleFilterName(const char* p_name, UpscaleFilter fallback);
	bool canUseXbr() const;        // hat die Maschine Shader und Bildpuffer?
	bool canUseSharpFit() const;
	void renderSprite(const Vec2i& position, const Vec2i& positionOnTexture, const Vec2i& size, const Vec4d& color, bool mirrorX = false, double rotation = 0.0, double scaling = 1.0);
	void renderSprite(Texture* p_sprite, const Vec2i& position, const Vec2i& positionOnTexture, const Vec2i& size, const Vec4d& color, bool mirrorX = false, double rotation = 0.0, double scaling = 1.0);
	SoundInstance* playSound(const std::string& filename, bool loop = false, double pitchSpectrum = 0.0, int priority = 0, bool forceCreation = false);

	void setBlendFunc(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha);

	void registerGameState(GameState* p_gs);
	GameState* findGameState(const std::string& gs);
	void setGameState(const std::string& gs, const ParameterBlock& context = ParameterBlock());
	void pushGameState(const std::string& gs, const ParameterBlock& context = ParameterBlock());
	GameState* popGameState(const ParameterBlock& context = ParameterBlock());
	GameState* getGameState();
	void processGameStateChanges();

	void playMusic(const std::string& filename, double loopBegin = 0.0);
	void stopMusic();

	bool isKeyDown(SDLKey key) const;
	bool wasKeyPressed(SDLKey key) const;
	bool wasKeyReleased(SDLKey key) const;
	void setKeyDown(SDLKey key, bool status);
	void setKeyPressed(SDLKey key, bool status);
	void setKeyReleased(SDLKey key, bool status);
	void setKeyData(SDLKey key, int data);

	Vec2i getCursorPosition() const;
	void setCursorPosition(const Vec2i& cursorPosition);
	bool isButtonDown(uint button) const;
	bool wasButtonPressed(uint button) const;
	bool wasButtonReleased(uint button) const;
	bool getKeyEvent(SDL_KeyboardEvent* p_out);
	bool isGUIFocused();
	void unfocusGUI();

	const std::vector<VirtualKey>& getVKs() const;
	const std::unordered_map<std::string, Action*>& getActions() const;
	const std::vector<Action*>& getActionsVector() const;
	int getKeyboardVK(SDLKey key) const;
	Action* registerAction(const std::string& name, int primary, int secondary = -1);
	void changeAction(const std::string& name, int primary, int secondary = -1);
	Action* getAction(const std::string& name) const;
	bool isActionDown(const std::string& name) const;
	bool wasActionPressed(const std::string& name) const;
	bool wasActionReleased(const std::string& name) const;
	void updateVKs();
	void updateActions();
	int getPressedVK(int timeOut = -1);
	void resetActions();
	void limitActionKeys();

	uint getLogicRate() const;
	void setLogicRate(uint logicRate);
	uint getFrameTime() const;
	uint getTime() const;

	const Vec2i& getScreenSize() const;
	const Vec2i& getScreenPow2Size() const;
	const Vec2i& getDisplaySize() const;

	void crossfade(Crossfade* p_crossfade, double duration, bool immediately = false);

	void loadConfig();
	void saveConfig();
	const std::string& getLanguage() const;
	void setLanguage(const std::string& language);
	double getSoundVolume() const;
	void setSoundVolume(double soundVolume);
	double getMusicVolume() const;
	void setMusicVolume(double musicVolume);
	bool wasVolumeChanged() const;
	int getDetails() const;
	void setDetails(int details);
	double getParticleDensity() const;
	void setParticleDensity(double particleDensity);

	void setMuteIcon(Texture* p_texture, const Vec2i& positionOnTexture, const Vec2i& size);
	void setRecordingIcon(Texture* p_texture, const Vec2i& positionOnTexture, const Vec2i& size);

	void loadStringDB(const std::string& filename);
	std::string localizeString(const std::string& text);
	std::string loadString(const std::string& id) const;

	AudioCapture* getAudioCapture();

	uint getTimePlayed() const { return timePlayed; }

private:
	Engine();
	~Engine();

	void setupCursor();

	// Setzt Fensterstil und -groesse, ohne SDLs Flags anzufassen. Der Stilwechsel
	// selbst ist Win32; die Groesse geht immer durch handleResize().
	void applyWindowStyle(bool wantFullScreen, const Vec2i& size);

	bool initialized;
	bool fullScreen;
	int fullScreenOverride;    // -1 = keine Vorgabe von der Kommandozeile
	bool swallowedReturn;      // Alt+Return verschluckt: das Loslassen auch
	Vec2i windowedSize;        // Groesse, auf die Vollbild-Aus zurueckfaellt
	long savedWindowStyle;     // Win32: der Stil vor dem Vollbild
	int savedWindowRect[4];    // Win32: x, y, w, h vor dem Vollbild
	SDL_Surface* p_display;
	PFNGLBLENDFUNCSEPARATEEXTPROC glExtBlendFuncSeparate;
	ALCdevice* p_audioDevice;
	AudioCapture* p_audioCapture;
	ALCcontext* p_audioContext;
	uint logicRate;
	bool modal;
	// These tables are indexed directly by SDL keysym. SDL 1.2's keysyms stop at
	// SDLK_LAST (323), so 512 slots were enough on Windows, but other SDL headers
	// number the same keys differently - Emscripten's use SDL2-style values
	// (scancode | 1<<10), which puts SDLK_LSHIFT at 1249 and SDLK_F7 at 1088.
	// Indexing a 512-entry table with those silently corrupts whatever follows it
	// in this object. Size for the largest keysym any supported SDL defines, and
	// range-check every index before use.
	static const int NUM_KEY_SLOTS = SDLK_LAST;   // 323 with SDL 1.2, 1536 with Emscripten's headers
	int keyData[NUM_KEY_SLOTS];
	int buttonData[NUM_KEY_SLOTS];
	std::vector<SDL_Joystick*> joysticks;
	std::vector<VirtualKey> virtualKeys;
	std::unordered_map<std::string, Action*> actions;
	std::vector<Action*> actionsVector;
	std::queue<SDL_KeyboardEvent> keyEventQueue;
	std::unordered_map<std::string, GameState*> gameStates;
	std::stack<GameState*> currentGameStates;
	uint frameTime;
	uint time;
	Vec2i screenSize;
	Vec2i screenPow2Size;
	Vec2i displaySize;
	Vec2i cursorPosition;
	int cursorImage[32][32];
	uint oldImageID;
	uint newImageID;
	Crossfade* p_crossfade;
	double crossfadeTime;
	double crossfadeDuration;
	StreamedSound* p_currentMusic;
	std::string currentMusicFilename;
	// Bildpuffer. frameTextureSize ist eine Zweierpotenz, weil WebGL 1 und
	// aeltere Treiber sonst NPOT-Texturen nur eingeschraenkt erlauben; benutzt
	// wird davon die linke untere Ecke in screenSize.
	uint frameBufferID;
	uint frameTextureID;
	uint frameDepthStencilID;
	Vec2i frameTextureSize;
	bool useFrameBuffer;
	UpscaleFilter upscaleFilter;
	uint xbrProgram;
	int xbrDecalLocation;
	int xbrTextureSizeLocation;
	int xbrSmallDetailsLocation;
	uint sharpFitProgram;
	int sharpFitDecalLocation;
	int sharpFitTextureSizeLocation;
	int sharpFitFrameSizeLocation;
	int sharpFitPrescaleLocation;
	uint xbrVertexBuffer;
	VideoRecorder* p_videoRecorder;
	uint recordingStartTime;
	uint lastRecordedFrameTimecode;

	GameState* p_stateToBeEntered;
	GameState* p_stateToGetFocus;
	GameState* p_stateToLoseFocus;
	std::list<GameState*> statesToBeLeft;
	ParameterBlock context;

	std::string language;
	double soundVolume;
	double musicVolume;
	bool volumeChanged;
	double oldSoundVolume;
	double oldMusicVolume;
	int details;
	double particleDensity;
	Texture* p_muteIconTexture;
	Vec2i muteIconPositionOnTexture;
	Vec2i muteIconSize;
	Texture* p_recordingIconTexture;
	Vec2i recordingIconPositionOnTexture;
	Vec2i recordingIconSize;

	std::unordered_map<std::string, std::string> stringDB;
	uint timePlayed;
	bool doScreenshot;
};

#endif