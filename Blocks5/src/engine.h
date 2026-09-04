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
	// Feuert die Aktion nach, solange die Taste liegt? Fuer das Laufen ja -
	// delay bis zur ersten Wiederholung, dann alle interval. Fuer einen
	// Schalter wie F12 nicht: der soll je Druck genau einmal ausloesen.
	bool repeats;
	int delay;
	int interval;
	int defaultPrimary;
	int defaultSecondary;
	std::vector<std::string> resetsActions;

	// Aus der Konfiguration gelesene Kennungen, die noch nicht aufgeloest werden
	// konnten. resolveActionKeys() traegt sie nach und leert die Felder wieder.
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

class Upscaler;
class U_Sharp;
class U_Smooth;
class U_SharpFit;
class U_Crt;

class Engine : public Singleton<Engine>
{
	friend class Singleton<Engine>;

public:
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
	// false, wenn kein Bild entstanden ist. Im Browser immer: GL_BGR ist dort
	// kein zulaessiges Format fuer glReadPixels, und SDL_SaveBMP_RW ist ein
	// abort().
	bool screenshot();

	// Der Bildpuffer, in den das Spiel rendert: immer 640x480, unabhaengig von
	// der Fenstergroesse. Jede Rechnung in Bildschirmkoordinaten bleibt gueltig.
	bool createFrameBuffer();
	void destroyFrameBuffer();
	void bindFrameBuffer();      // Ziel = Bildpuffer, Viewport 640x480
	void unbindFrameBuffer();    // Ziel = Fenster
	void presentFrame();         // Bildpuffer -> Fenster, mit schwarzen Balken

	// Ein zweites Ziel: in eine Textur zeichnen statt in den Bildpuffer.
	// Dazwischen ist (0,0) die linke obere Ecke der Textur; danach steht alles
	// wieder so, wie der Rest des Bildes es erwartet. false heisst, dass diese
	// Maschine es nicht kann - dann wurde auch nichts angefasst.
	bool beginRenderToTexture(uint textureID, const Vec2i& size);
	void endRenderToTexture();

	// Die eine Textur, in die hineingezeichnet wird. Es gibt genau eine, weil
	// es genau einen Nutzer gibt - den Hinweiszettel -, und von dem ist immer
	// nur einer zu sehen. Sie gehoert der Engine und nicht ihm: sie faellt mit
	// dem Bildpuffer, also solange der GL-Kontext noch steht, waehrend ein
	// Objekt erst abgebaut wird, wenn er laengst weg ist. 0, wenn es nicht
	// geht.
	uint getOffscreenTexture(const Vec2i& size);
	// Wohin im Fenster das 640x480-Bild kommt: mittig, Seitenverhaeltnis
	// erhalten. Auch die Umkehrung fuer die Mausposition benutzt genau das.
	void computePresentRect(int& x, int& y, int& w, int& h) const;

	// Vollbild ist nur eine besondere Groesse plus ein Stilwechsel am
	// Win32-Fenster vorbei an SDL; SDLs Flags bleiben deshalb das ganze Programm
	// ueber SDL_OPENGL | SDL_RESIZABLE, sonst stirbt der GL-Kontext.
	void overrideFullScreen(bool wantFullScreen) { fullScreenOverride = wantFullScreen ? 1 : 0; }

	// -nosplash. Wie overrideFullScreen() vor init() aufzurufen.
	void skipSplash() { splashSkipped = true; }
	bool isSplashSkipped() const { return splashSkipped; }
	void handleResize(int width, int height);   // auf SDL_VIDEORESIZE hin
	// Alles vergessen, was an Tasten und Maustasten aufgelaufen ist: nach allem,
	// was die Hauptschleife angehalten hat, ist der Eingabezustand unbrauchbar.
	void flushInput();

#ifdef _WIN32
	// Zieht der Benutzer am Fensterrand, laeuft die Hauptschleife nicht - Windows
	// haelt sie in einer eigenen Nachrichtenschleife fest. Das hier zeichnet das
	// zuletzt gerenderte Bild in der neuen Groesse noch einmal, ohne Spiellogik.
	void repaintDuringSizeMove();
	void setInSizeMove(bool value) { inSizeMove = value; }
	bool isInSizeMove() const { return inSizeMove; }

	// Dasselbe fuer den Dateidialog: auch der bringt eine fremde
	// Nachrichtenschleife mit, und das Spielfenster bliebe sonst schwarz.
	void beginForeignMessageLoop();
	void endForeignMessageLoop();

	// Kleinste Fenstergroesse, die handleResize() zulaesst, als Fensterrechteck
	// samt Rahmen - fuer WM_GETMINMAXINFO.
	Vec2i getMinimumWindowSize() const;
#endif
	void setFullScreen(bool wantFullScreen);
	void toggleFullScreen() { setFullScreen(!fullScreen); }
	bool isFullScreen() const { return fullScreen; }
#ifdef __EMSCRIPTEN__
	// Auf einem Telefon holt sich das Spiel das Vollbild bei jeder Beruehrung
	// selbst zurueck. Gerufen wird das aus dem DOM-Rueckruf und nirgends sonst:
	// die Fullscreen-API verlangt eine echte Geste, und die Ereignisse aus der
	// Animationsschleife sind keine.
	void enforceTouchFullScreen();
#endif
	Vec2i getDesktopSize() const;
	// Was ein frisch installiertes Spiel als Fenstergroesse bekommt: das groesste
	// ganzzahlige Vielfache von 640x480, das noch bequem auf den Schirm passt.
	Vec2i getDefaultWindowSize() const;

	// Der GL-Zustand der Filter: der gemeinsame Vertexpuffer und, wo einer
	// gebraucht wird, das uebersetzte Programm. Laesst sich eines nicht
	// uebersetzen, meldet sich dieser Filter als nicht verfuegbar und die
	// Anzeige faellt auf "Scharf" zurueck; das Spiel laeuft in jedem Fall.
	void createUpscalerGL();
	void destroyUpscalerGL();

	// getUpscaler() ist der Wunsch aus der config.xml, getEffectiveUpscaler()
	// das, was auf dieser Maschine davon uebrig bleibt.
	void setUpscaler(Upscaler* p_upscaler);
	Upscaler* getUpscaler() const { return p_wantedUpscaler; }
	Upscaler* getEffectiveUpscaler() const;
	// Alle vier, in der Reihenfolge, in der sie im Optionsdialog stehen.
	const std::vector<Upscaler*>& getUpscalers() const { return upscalers; }
	// Den Filter zu seinem Namen aus der config.xml; 0, wenn keiner so heisst.
	Upscaler* findUpscaler(const char* p_name) const;
	// Die Roehre beim Namen. Zwei Stellen brauchen genau sie und keinen
	// beliebigen Filter: der Optionsdialog stellt ihre sechs Regler, und das
	// Hauptmenue bietet sie einmalig an.
	U_Crt& getCrt() const { return *p_crt; }
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
	// Nimmt einer Taste das "in diesem Bild gedrueckt"-Kennzeichen weg, weil
	// GUI::update() vor den Spielzustaenden laeuft und beide sie saehen.
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
	// Das naechste Tastenereignis. p_repeat sagt, ob es von SDLs
	// Tastenwiederholung stammt und nicht von einem neuen Druck: wer die Taste
	// als Befehl liest - Escape, Return, die Kuerzel der Editoren -, muss so
	// eines ueberspringen, sonst loest ein liegender Finger den Befehl alle
	// 60 ms erneut aus. Ein Textfeld und eine Liste wollen es dagegen haben.
	bool getKeyEvent(SDL_KeyboardEvent* p_out, bool* p_repeat = 0);
	bool isGUIFocused();
	void unfocusGUI();

	const std::vector<VirtualKey>& getVKs() const;
	const std::unordered_map<std::string, Action*>& getActions() const;
	const std::vector<Action*>& getActionsVector() const;
	int getKeyboardVK(SDLKey key) const;

	// Die Kennung, unter der eine Taste in der config.xml steht. Die VK-Nummer
	// taugt dafuer nicht: sie ist ein Index in virtualKeys und haengt an
	// SDLK_LAST und daran, welche Joysticks beim Start angeschlossen waren.
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
	// Auf einen Tastendruck warten, zum Belegen einer Aktion. Ein Zustand und
	// keine eigene Schleife: im Browser fuellt erst die Rueckkehr zur Seite die
	// Ereignisschlange. Solange er laeuft, gehoert die Tastatur ihm allein.
	enum
	{
		GRAB_WAITING   = -3,   // laeuft noch, nichts entschieden
		GRAB_CANCELLED = -2,   // Escape: die Belegung bleibt, wie sie war
		GRAB_NO_KEY    = -1    // Zeit abgelaufen; heisst "keine Taste"
	};

	// timeOutMS <= 0 wartet ohne Frist.
	void beginKeyGrab(int timeOutMS = 3000);
	bool isGrabbingKey() const;

	// Liefert GRAB_WAITING, solange nichts entschieden ist; sonst einmal das
	// Ergebnis und stellt die Wartestellung damit ab.
	int pollKeyGrab();
	void resetActions();

	// Nur diese eine Aktion auf ihre Vorgabe zuruecksetzen.
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
	bool isAppActive() const;
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

	// Kurze Meldung, die oben ins Bild faehrt. duration ist die Standzeit in
	// Sekunden ohne das Ein- und Ausfahren; 0 nimmt den Wert der Art. Ein Fehler
	// spielt teleport_failed.ogg, sofern suppressSound das nicht unterbindet.
	// Dieselbe Meldung ein zweites Mal verlaengert nur die Standzeit der ersten.
	enum ToastType
	{
		TOAST_OK = 0,
		TOAST_ERROR
	};

	void showToast(ToastType type, const std::string& text, double duration = 0.0, bool suppressSound = false);

private:
	Engine();
	~Engine();

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

	void setupCursor();
	void updateToasts();
	void renderToasts();

	// Verteilt die Plaetze neu: die neueste ganz oben, die aelteren darunter.
	// Wer schon hinausfaehrt, zaehlt nicht mehr mit.
	void reflowToasts();

	void updateKeyGrab();
	void syncActionDown(Action& action);

	// Das zuletzt gezeichnete Bild noch einmal auf den Schirm bringen: aus dem
	// Bildpuffer, mit Balken und Filter. Ohne Logiktakt und ohne neu zu zeichnen.
	void showLastFrame();

	// Setzt Fensterstil und -groesse, ohne SDLs Flags anzufassen. Der Stilwechsel
	// selbst ist Win32; die Groesse geht immer durch handleResize().
	void applyWindowStyle(bool wantFullScreen, const Vec2i& size);

	void rememberWindowPlacement();   // liest Position/Groesse vom Fenster
	bool isWindowMaximized() const;   // maximiert? dann nichts nachfuehren
	void restoreWindowPosition();     // setzt sie beim Start wieder
#ifdef _WIN32
	void hookWindowProc();            // eigene Fensterprozedur davorschalten
	void unhookWindowProc();          // und wieder herausnehmen
#endif

	// Die Abbildung des gerade wirksamen Filters, in beide Richtungen; die
	// Koordinaten laufen von -1 bis 1 ab der Bildmitte. Nur die Roehre verzieht
	// wirklich etwas, alle anderen geben zurueck, was sie bekommen haben.
	Vec2d warpToSource(const Vec2d& p) const;
	Vec2d warpToOutput(const Vec2d& p) const;

	// Aeltester zuerst, also wird auch in dieser Reihenfolge gezeichnet: eine
	// sterbende Meldung faehrt hinter ihre juengere Nachbarin und nicht darueber.
	std::list<Toast> toasts;

	// Die Wartestellung auf einen Tastendruck. grabOldState haelt fest, was beim
	// Anstellen schon gedrueckt war - gesucht ist die *neu* heruntergehende.
	bool grabbingKey;
	int grabResult;
	uint grabDeadline;
	bool grabHasDeadline;
	std::vector<bool> grabOldState;

	bool initialized;
	bool fullScreen;
	int fullScreenOverride;    // -1 = keine Vorgabe von der Kommandozeile
	bool splashSkipped;        // -nosplash
	bool swallowedReturn;      // Alt+Return verschluckt: das Loslassen auch
	Vec2i windowedSize;        // Groesse, auf die Vollbild-Aus zurueckfaellt
	Vec2i windowedPosition;    // dito fuer die Position
	bool  windowedPositionKnown;
	bool  maximized;           // war das Fenster beim Beenden maximiert?
#ifdef _WIN32
	bool inSizeMove;           // Benutzer haelt gerade Rand oder Titel
#endif
	long savedWindowStyle;     // Win32: der Stil vor dem Vollbild
	int savedWindowRect[4];    // Win32: x, y, w, h vor dem Vollbild
	SDL_Surface* p_display;
	PFNGLBLENDFUNCSEPARATEEXTPROC glExtBlendFuncSeparate;
	ALCdevice* p_audioDevice;
	AudioCapture* p_audioCapture;
	ALCcontext* p_audioContext;
	uint logicRate;
	// Die Tabellen werden direkt mit dem SDL-Keysym indiziert, und der zaehlt je
	// nach SDL-Kopf anders. Deshalb SDLK_LAST als Groesse und jeder Index geprueft.
	static const int NUM_KEY_SLOTS = SDLK_LAST;   // 323 unter SDL 1.2, 1536 mit Emscriptens Koepfen
	int keyData[NUM_KEY_SLOTS];
	// Liegt die Taste gerade wirklich unter einem Finger? Das steht hier und
	// nicht als Bit in keyData, weil keyData zweimal von aussen genullt wird -
	// flushInput() leert es, und GS_Menu::onUpdate ueberschreibt es jeden Takt
	// mit der aufgezeichneten Demo. Ein Zustand, der ueber mehrere Takte halten
	// muss, kann dort nicht stehen.
	bool keyHeld[NUM_KEY_SLOTS];
	int buttonData[NUM_KEY_SLOTS];
	std::vector<SDL_Joystick*> joysticks;
	std::vector<VirtualKey> virtualKeys;
	std::unordered_map<std::string, Action*> actions;
	std::vector<Action*> actionsVector;
	// Das Tastenereignis und ob es die Wiederholung einer liegenden Taste ist.
	// Ein Textfeld will die Wiederholung, ein Befehl nicht - siehe
	// Engine::getKeyEvent().
	struct QueuedKeyEvent
	{
		SDL_KeyboardEvent event;
		bool repeat;
	};
	std::queue<QueuedKeyEvent> keyEventQueue;
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
	// Bildpuffer. frameTextureSize ist eine Zweierpotenz, weil WebGL 1 NPOT-
	// Texturen nur eingeschraenkt erlaubt; benutzt wird die linke untere Ecke.
	uint frameBufferID;
	uint frameTextureID;
	uint frameDepthStencilID;
	Vec2i frameTextureSize;
	uint renderTargetID;       // der Rahmen zum Hineinzeichnen, ohne feste Textur
	uint offscreenTextureID;
	Vec2i offscreenTextureSize;
	bool renderTargetScissor;  // war der Schnittrahmen an, als es losging?
	bool useFrameBuffer;
	// Die vier Filter. upscalers besitzt sie und haelt die Reihenfolge des
	// Optionsdialogs; die vier Zeiger daneben sind die Abkuerzung dorthin.
	std::vector<Upscaler*> upscalers;
	U_Sharp* p_sharp;
	U_Smooth* p_smooth;
	U_SharpFit* p_sharpFit;
	U_Crt* p_crt;
	Upscaler* p_wantedUpscaler;
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
	// Fokus gewonnen oder verloren, aus welchem Ereignis auch immer.
	void handleAppFocus(bool gained);

	// Hat das Fenster den Fokus? Ein Mitglied und keine Schleifenvariable, weil
	// emscripten_set_main_loop je Bild einmal aufruft und nichts auf dem Stapel
	// stehen bleibt - und weil der Testhaken es meldet.
	bool appActive;

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