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
class Sprites;

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

	// Aus der Konfiguration gelesene Kennungen, die noch nicht aufgeloest
	// werden konnten: Engine::loadConfig() laeuft, bevor virtualKeys gefuellt
	// ist. resolveActionKeys() traegt sie nach, sobald die Liste steht, und
	// leert die Felder wieder.
	std::string pendingPrimaryId;
	std::string pendingSecondaryId;

	int data;
	int countDown;
	int buffered;
};

struct VirtualKey
{
	// name wird angezeigt und kommt bei Tasten von SDL; id steht in der
	// config.xml und muss deshalb ueberall dasselbe bedeuten.
	std::string name;
	std::string id;
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
	UF_CRT            // Roehrenmonitor: Maske, Streifen, Hof, gewoelbte Scheibe
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

	// -nosplash laesst Logo, Jingle und die drei Sekunden davor aus und geht
	// sofort ins Laden. Zum Entwickeln gedacht, wo man den Start oft sieht;
	// gehoert wie overrideFullScreen() vor init().
	void skipSplash() { splashSkipped = true; }
	bool isSplashSkipped() const { return splashSkipped; }
	void handleResize(int width, int height);   // auf SDL_VIDEORESIZE hin
	// Alles vergessen, was an Tasten und Maustasten aufgelaufen ist. Nach
	// etwas, das die Hauptschleife angehalten hat, ist der Eingabezustand
	// nicht vertrauenswuerdig: der Windows-Dateidialog hat Ereignisse fuer
	// sein eigenes Fenster weitergereicht, die das Spiel als frischen Klick
	// lesen wuerde, und getPressedVK() laesst die Taste liegen, mit der es
	// gerade beendet wurde. Kein Windows-Code, nur SDL - deshalb steht es
	// vor dem Block und nicht darin.
	void flushInput();

#ifdef _WIN32
	// Zieht der Benutzer am Fensterrand, laeuft die Hauptschleife nicht:
	// Windows haelt sie in einer eigenen Nachrichtenschleife fest. Das hier
	// zeichnet das zuletzt gerenderte Bild in der neuen Groesse noch einmal,
	// damit wenigstens die Skalierung mitkommt. Keine Spiellogik.
	void repaintDuringSizeMove();
	void setInSizeMove(bool value) { inSizeMove = value; }
	bool isInSizeMove() const { return inSizeMove; }

	// Dasselbe fuer den Dateidialog: auch der bringt eine fremde
	// Nachrichtenschleife mit, und auch dann steht die Hauptschleife. Ohne
	// das bliebe das Spielfenster schwarz, solange der Dialog offen ist.
	void beginForeignMessageLoop();
	void endForeignMessageLoop();

	// Kleinste Fenstergroesse, die handleResize() zulaesst, als Fensterrechteck
	// samt Rahmen - fuer WM_GETMINMAXINFO.
	Vec2i getMinimumWindowSize() const;
#endif
	void setFullScreen(bool wantFullScreen);
	void toggleFullScreen() { setFullScreen(!fullScreen); }
	bool isFullScreen() const { return fullScreen; }
	Vec2i getDesktopSize() const;
	// Was ein frisch installiertes Spiel als Fenstergroesse bekommt: das
	// groesste ganzzahlige Vielfache von 640x480, das noch bequem auf den
	// Bildschirm passt.
	Vec2i getDefaultWindowSize() const;

	// Die beiden Shader des Spiels: src/sharpfit_shader.h und src/crt_shader.h.
	// Laesst sich einer nicht uebersetzen, faellt seine Anzeige auf UF_NEAREST
	// zurueck; das Spiel laeuft in jedem Fall.
	bool createPresentPrograms();
	void destroyPresentPrograms();

	// getUpscaleFilter() liefert den *Wunsch* - das, was der Spieler gewaehlt
	// hat und was in der config.xml steht. getEffectiveUpscaleFilter() liefert,
	// was tatsaechlich gezeichnet wird: ohne uebersetztes Programm wird aus
	// sharp-fit oder CRT nearest. Der Wunsch bleibt stehen, damit dieselbe config.xml auf
	// einer Maschine mit Shadern wieder das Richtige tut.
	void setUpscaleFilter(UpscaleFilter filter);
	UpscaleFilter getUpscaleFilter() const { return upscaleFilter; }
	UpscaleFilter getEffectiveUpscaleFilter() const;
	// Die Namen, unter denen der Filter in der config.xml steht.
	static const char* getUpscaleFilterName(UpscaleFilter filter);
	static UpscaleFilter parseUpscaleFilterName(const char* p_name, UpscaleFilter fallback);
	bool canUseSharpFit() const;   // hat die Maschine Shader und Bildpuffer?
	bool canUseCrt() const;

	// Die beiden Regler des Roehrenfilters, je 0..1. Sie stehen in der
	// config.xml und wirken sofort - der Shader liest sie jedes Bild neu.
	// crtScanline: 0 = VGA-Monitor ohne Luecken, 1 = Konsolenstreifen.
	// crtCurvature: 0 = flache Scheibe, 1 = volle Woelbung. Die Woelbung geht
	// auch durch die Mausumrechnung, siehe warpToSource/warpToOutput.
	double getCrtScanline() const { return crtScanline; }
	double getCrtCurvature() const { return crtCurvature; }
	double getCrtBloom() const { return crtBloom; }
	double getCrtFlicker() const { return crtFlicker; }
	double getCrtScanFlicker() const { return crtScanFlicker; }
	void setCrtScanline(double value);
	void setCrtCurvature(double value);
	void setCrtBloom(double value);
	void setCrtFlicker(double value);
	void setCrtScanFlicker(double value);
	void renderSprite(const Vec2i& position, const Vec2i& positionOnTexture, const Vec2i& size, const Vec4d& color, bool mirrorX = false, double rotation = 0.0, double scaling = 1.0);
	void renderSprite(Texture* p_sprite, const Vec2i& position, const Vec2i& positionOnTexture, const Vec2i& size, const Vec4d& color, bool mirrorX = false, double rotation = 0.0, double scaling = 1.0);

	// Alle Teilbilder eines Objekts zeichnen. color ist die Farbe des
	// Renderdurchgangs; die Eigenfaerbung jedes Teilbilds kommt dazu.
	void renderSprites(const Sprites& sprites, const Vec4d& color);
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
	// Nimmt einer Taste das "in diesem Bild gedrueckt"-Kennzeichen weg. Die
	// GUI verteilt Tastenereignisse ueber eine eigene Warteschlange, die
	// Spielzustaende fragen daneben wasKeyPressed() ab - und GUI::update()
	// laeuft zuerst. Ohne das hier saehe das Hauptmenue das Escape, mit dem
	// der Optionsdialog sich gerade selbst geschlossen hat, und beendete das
	// Spiel.
	void consumeKeyPress(SDLKey key);
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

	// Die Kennung, unter der eine Taste in der config.xml steht. Die VK-Nummer
	// taugt dafuer nicht: sie ist ein Index in virtualKeys, und der haengt an
	// SDLK_LAST (323 unter SDL 1.2, 1536 im Browser) und daran, welche
	// Joysticks beim Start angeschlossen waren.
	const std::string& getVKId(int vk) const;
	int getVKFromId(const std::string& id) const;
	void resolveActionKeys();
	void repairLostBindings();
	Action* registerAction(const std::string& name, int primary, int secondary = -1);
	void changeAction(const std::string& name, int primary, int secondary = -1);
	Action* getAction(const std::string& name) const;
	bool isActionDown(const std::string& name) const;
	bool wasActionPressed(const std::string& name) const;
	bool wasActionReleased(const std::string& name) const;
	void updateVKs();
	void updateActions();
	// Wartet auf einen Tastendruck und liefert die virtuelle Taste dazu.
	// Escape bricht ab und liefert VK_CANCELLED - der Aufrufer laesst die
	// Belegung dann, wie sie war. Laeuft die Zeit ab, kommt -1, und das heisst
	// "keine Taste": so raeumt man eine Belegung weg.
	//
	// Die Hauptschleife steht solange still, deshalb zeichnet die Warteschleife
	// selbst weiter - sonst froere das Bild fuer bis zu drei Sekunden ein, und
	// die Aufschrift, die zum Tastendruck auffordert, kaeme nie auf den Schirm.
	static const int VK_CANCELLED = -2;
	int getPressedVK(int timeOut = -1);
	void resetActions();

	// Nur diese eine Aktion auf ihre Vorgabe zuruecksetzen. Wer sich eine
	// Taste verlegt hat, musste bisher das ganze Schema wegwerfen.
	void resetAction(const std::string& name);
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
	// Was das System spricht, auf "de" oder "en" heruntergebrochen. Wird nur
	// gefragt, wenn die config.xml gar keine Sprache nennt - siehe loadConfig().
	static std::string detectSystemLanguage();
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
	void saveTimePlayed();

	// Kurze Meldung, die oben ins Bild faehrt, eine Weile stehen bleibt und
	// wieder hinausfaehrt. Sie gehoert der Engine und nicht einem Spielstatus,
	// weil sie ueber allem liegt: die Levelauswahl, beide Editoren und das
	// Hauptmenue benutzen dieselbe.
	//
	// duration ist die Standzeit in Sekunden, ohne das Ein- und Ausfahren;
	// 0 nimmt den Wert, der zur Art passt (TOAST_SECONDS_OK/ERROR). Eine
	// Fehlermeldung spielt teleport_failed.ogg, sofern suppressSound das nicht
	// unterbindet. Eine Erfolgsmeldung ist immer still - deshalb heisst der
	// Schalter so herum: es gibt keinen Ton, den er einschalten koennte.
	//
	// Gibt es dieselbe Meldung derselben Art schon - und faehrt sie noch nicht
	// gerade hinaus -, entsteht keine zweite. Stattdessen wird ihre Standzeit
	// auf das Laengere von beidem gesetzt. Der Ton kommt trotzdem: er ist die
	// Antwort auf den Klick, nicht auf die Meldung.
	enum ToastType
	{
		TOAST_OK = 0,
		TOAST_ERROR
	};

	void showToast(ToastType type, const std::string& text, double duration = 0.0, bool suppressSound = false);

private:
	Engine();
	~Engine();

	void setupCursor();

	// Eine Meldung im Stapel. Es gibt drei Abschnitte: hereinfahren, stehen,
	// hinausfahren. phaseTime laeuft je Abschnitt neu los.
	struct Toast
	{
		ToastType type;
		std::string text;
		int phase;          // 0 = herein, 1 = stehen, 2 = hinaus
		uint phaseTime;     // ms in diesem Abschnitt
		uint duration;      // ms fuer Abschnitt 1
		double y;           // wo die Meldung gerade liegt
		double targetY;     // wohin sie will
	};

	// Aeltester zuerst. Damit wird auch in dieser Reihenfolge gezeichnet, und
	// die neueren liegen oben - eine sterbende Meldung faehrt hinter ihre
	// juengere Nachbarin und nicht ueber sie.
	std::list<Toast> toasts;

	void updateToasts();
	void renderToasts();

	// Das zuletzt gezeichnete Bild noch einmal auf den Schirm bringen: aus dem
	// Bildpuffer, mit Balken und Filter, und tauschen. Ohne Logiktakt und ohne
	// neu zu zeichnen.
	void showLastFrame();

	// Ein Bild zeichnen und zeigen, ohne Logiktakt - dasselbe, was die
	// Hauptschleife an ihrem Fuss tut. Fuer die Stellen, an denen sie
	// selbst steht und der Schirm trotzdem stimmen muss.
	void renderAndPresent();
	// Verteilt die Plaetze neu: die neueste ganz oben, die aelteren darunter.
	// Wer schon hinausfaehrt, zaehlt nicht mehr mit.
	void reflowToasts();

	// Setzt Fensterstil und -groesse, ohne SDLs Flags anzufassen. Der Stilwechsel
	// selbst ist Win32; die Groesse geht immer durch handleResize().
	void applyWindowStyle(bool wantFullScreen, const Vec2i& size);

	bool initialized;
	bool fullScreen;
	int fullScreenOverride;    // -1 = keine Vorgabe von der Kommandozeile
	bool splashSkipped;        // -nosplash
	bool swallowedReturn;      // Alt+Return verschluckt: das Loslassen auch
	Vec2i windowedSize;        // Groesse, auf die Vollbild-Aus zurueckfaellt
	Vec2i windowedPosition;    // dito fuer die Position
	bool  windowedPositionKnown;
	bool  maximized;           // war das Fenster beim Beenden maximiert?
	void rememberWindowPlacement();   // liest Position/Groesse vom Fenster
	bool isWindowMaximized() const;   // maximiert? dann nichts nachfuehren
	void restoreWindowPosition();     // setzt sie beim Start wieder
#ifdef _WIN32
	void hookWindowProc();            // eigene Fensterprozedur davorschalten
	void unhookWindowProc();          // und wieder herausnehmen
	bool inSizeMove;                  // Benutzer haelt gerade Rand oder Titel
#endif
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
	// Beide Praesentiershader teilen sich den Vertexshader, den Vertexpuffer und
	// vier Uniforms; der Roehrenshader hat zwei weitere. Ein Stueck Struktur
	// spart sechs weitere gleichnamige Felder.
	struct PresentProgram
	{
		uint program;
		int decal, textureSize, frameSize, prescale;
		int scanline, curvature, bloom, flicker, time, scanPhase, scanFlicker;   // nur UF_CRT, sonst -1
	};
	PresentProgram sharpFit;
	PresentProgram crt;
	double crtScanline;
	double crtCurvature;
	double crtBloom;
	double crtFlicker;
	double crtScanFlicker;
	// Dieselbe Abbildung wie im Roehrenshader, in beide Richtungen. Die
	// Koordinaten laufen von -1 bis 1 ab der Bildmitte. warpToSource ist die
	// Formel selbst - Ausgabepunkt zu Quellpunkt, so wie der Shader rechnet -,
	// warpToOutput ihre Umkehrung. Siehe src/crt_shader.h.
	Vec2d warpToSource(const Vec2d& p) const;
	Vec2d warpToOutput(const Vec2d& p) const;
	bool createPresentProgram(PresentProgram& target, const char* p_fragmentSource,
							  const char* p_name);
	void destroyPresentProgram(PresentProgram& target);
	uint presentVertexBuffer;
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