#ifndef _ENGINE_H
#define _ENGINE_H

/*** The Engine class ***/

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
	// Does the action keep firing while the key is held? For walking yes -
	// delay until the first repeat, then every interval. Not for a toggle like
	// F12: that one must fire exactly once per press.
	bool repeats;
	int delay;
	int interval;
	int defaultPrimary;
	int defaultSecondary;
	std::vector<std::string> resetsActions;

	// Ids read from the config that could not be resolved yet.
	// resolveActionKeys() fills them in and clears these members again.
	std::string pendingPrimaryId;
	std::string pendingSecondaryId;

	int data;
	int countDown;
	int buffered;
};

struct VirtualKey
{
	// name is what SDL calls it and id is what stands in config.xml, which must
	// mean the same everywhere and so can never be translated. niceName is the
	// third thing, and the only one a player ever reads: a $ID for a key that
	// has a localized name, the plain text for one that does not, and for a
	// joystick the part after the device word. It is an id and not the finished
	// text because the language can change while the game runs.
	std::string name;
	std::string id;
	std::string niceName;
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
	// false where no image could be produced. In the browser it goes to the
	// player as a download instead of as a file in the user directory.
	bool screenshot();

	// The framebuffer the game renders into: always 640x480, whatever the
	// window size. Every calculation in screen coordinates stays valid.
	bool createFrameBuffer();
	void destroyFrameBuffer();
	void bindFrameBuffer();      // target = framebuffer, viewport 640x480
	void unbindFrameBuffer();    // target = window
	void presentFrame();         // framebuffer -> window, with black bars

	// A second target: draw into a texture instead of into the framebuffer.
	// In between, (0,0) is the top left corner of the texture; afterwards
	// everything is back the way the rest of the frame expects it. false means
	// this machine cannot do it - and then nothing has been touched either.
	bool beginRenderToTexture(uint textureID, const Vec2i& size);
	void endRenderToTexture();

	// Borrow a texture to draw into and hand it back again. The textures
	// belong to the Engine and not to the borrower: they fall with the
	// framebuffer, that is while the GL context still stands, whereas an object
	// is torn down only long after that context is gone. acquire gives 0 where
	// it does not work.
	//
	// A pool and not a single texture, because several hint notes can be
	// visible at once - stepping from one onto its neighbour fades the one out
	// while the other fades in. With a shared texture the second would draw
	// into it while the first still read from it, and both would then show the
	// same text. The pool grows only up to the largest number of textures
	// borrowed at once.
	uint acquireOffscreenTexture(const Vec2i& size);
	void releaseOffscreenTexture(uint textureID);
	// Where in the window the 640x480 picture goes: centred, aspect kept. The
	// inverse for the mouse position uses exactly this too.
	void computePresentRect(int& x, int& y, int& w, int& h) const;

	// Fullscreen is nothing but a special size plus a style change on the
	// Win32 window behind SDL's back; SDL's flags therefore stay
	// SDL_OPENGL | SDL_RESIZABLE for the whole life of the process, or the GL
	// context dies.
	void overrideFullScreen(bool wantFullScreen) { fullScreenOverride = wantFullScreen ? 1 : 0; }

	// -nosplash. To be called before init(), like overrideFullScreen().
	void skipSplash() { splashSkipped = true; }
	bool isSplashSkipped() const { return splashSkipped; }

	// -nofbo and -noshader: force the two fallback paths that otherwise only
	// old hardware takes. Without a framebuffer object the game draws unscaled
	// into the back buffer, there is no upscaling filter, no crossfade and no
	// rolled hint note; without shaders SharpFit and the CRT filter stay
	// unavailable. Neither can be checked other than by hand, because no
	// machine here is that old - hence the switches. To be set before init()
	// as well.
	void disableFrameBuffer() { frameBufferDisabled = true; }
	void disableShaders() { shadersDisabled = true; }
	void handleResize(int width, int height);   // on SDL_VIDEORESIZE
	// Forget everything that has piled up in keys and mouse buttons: after
	// anything that stopped the main loop, the input state is useless.
	void flushInput();

#ifdef _WIN32
	// While the user drags the window border the main loop does not run -
	// Windows holds it in a message loop of its own. This draws the last
	// rendered frame once more at the new size, with no game logic.
	void repaintDuringSizeMove();
	void setInSizeMove(bool value) { inSizeMove = value; }
	bool isInSizeMove() const { return inSizeMove; }

	// The same for the file dialog: it too brings a foreign message loop with
	// it, and the game window would otherwise stay black.
	void beginForeignMessageLoop();
	void endForeignMessageLoop();

	// Smallest window size handleResize() allows, as a window rect including
	// the frame - for WM_GETMINMAXINFO.
	Vec2i getMinimumWindowSize() const;
	// Without a framebuffer object the window stays at 640x480 - see
	// fixWindowSize(). The window procedure asks in order to give Windows the
	// maximum as well.
	bool hasFixedWindowSize() const { return !useFrameBuffer; }
#endif
	void setFullScreen(bool wantFullScreen);
	void toggleFullScreen() { setFullScreen(!fullScreen); }
	bool isFullScreen() const { return fullScreen; }
#ifdef __EMSCRIPTEN__
	// On a phone the game takes the fullscreen back itself on every touch.
	// Called from the DOM callback and nowhere else: the Fullscreen API needs a
	// real user gesture, and the events out of the animation loop are not one.
	void enforceTouchFullScreen();

	// A coarse pointer and no fine one: a notebook with a touchscreen is not a
	// phone. The rule itself lives in pre.js, where the page needs it too.
	bool isPhone() const;
#endif
	Vec2i getDesktopSize() const;
	// The window size a freshly installed game gets: the largest integer
	// multiple of 640x480 that still fits comfortably on the screen.
	Vec2i getDefaultWindowSize() const;

	// The filters' GL state: the shared vertex buffer and, where one is
	// needed, the compiled program. If one will not compile, that filter
	// reports itself unavailable and the presentation falls back to Sharp; the
	// game runs either way.
	void createUpscalerGL();
	void destroyUpscalerGL();

	// getUpscaler() is the wish out of config.xml, getEffectiveUpscaler() what
	// is left of it on this machine.
	void setUpscaler(Upscaler* p_upscaler);
	Upscaler* getUpscaler() const { return p_wantedUpscaler; }
	Upscaler* getEffectiveUpscaler() const;
	// All four, in the order they appear in the options dialog.
	const std::vector<Upscaler*>& getUpscalers() const { return upscalers; }
	// The filter for its name out of config.xml; 0 if none is called that.
	Upscaler* findUpscaler(const char* p_name) const;
	// The CRT filter by name. Two places need exactly this filter and not just
	// any one: the options dialog sets its six sliders, and the main menu
	// offers it once.
	U_Crt& getCrt() const { return *p_crt; }
	void renderSprite(const Vec2i& position, const Vec2i& positionOnTexture, const Vec2i& size, const Vec4d& color, bool mirrorX = false, double rotation = 0.0, double scaling = 1.0);
	void renderSprite(Texture* p_sprite, const Vec2i& position, const Vec2i& positionOnTexture, const Vec2i& size, const Vec4d& color, bool mirrorX = false, double rotation = 0.0, double scaling = 1.0);

	// Draw all sprites of an object. color is the colour of the render pass;
	// each sprite's own tint comes on top of it.
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

	void playMusic(const std::string& filename, double loopBegin = 0.0, bool resumeWhereStopped = false);
	void stopMusic();

	bool isKeyDown(SDLKey key) const;
	bool wasKeyPressed(SDLKey key) const;
	// Takes the "pressed in this frame" flag off a key, because GUI::update()
	// runs before the game states and both would see it.
	void consumeKeyPress(SDLKey key);
	bool wasKeyReleased(SDLKey key) const;
	// Any key, any mouse button - for a caller that only wants to know that
	// somebody pressed something at all.
	bool wasAnyKeyPressed() const;
	bool wasAnyButtonPressed() const;
	void setKeyDown(SDLKey key, bool status);
	void setKeyPressed(SDLKey key, bool status);
	void setKeyReleased(SDLKey key, bool status);
	void setKeyData(SDLKey key, int data);

	Vec2i getCursorPosition() const;
	// Where the window reports the cursor: without the mapping onto the
	// 640x480 picture and without the CRT filter's barrel distortion. That is
	// how to tell whether the mouse moved or only the mapping changed -
	// GUI::update() depends on it.
	const Vec2i& getRawCursorPosition() const;
	void setCursorPosition(const Vec2i& cursorPosition);
	bool isButtonDown(uint button) const;
	bool wasButtonPressed(uint button) const;
	bool wasButtonReleased(uint button) const;
	// The next key event. p_repeat says whether it comes from SDL's key repeat
	// rather than from a fresh press: anything that reads the key as a command
	// - Escape, Return, the editors' shortcuts - must skip such an event, or a
	// finger left on the key fires the command again every 60 ms. An edit box
	// and a list, on the other hand, want it.
	bool getKeyEvent(SDL_KeyboardEvent* p_out, bool* p_repeat = 0);
	bool isGUIFocused();
	void unfocusGUI();

	const std::vector<VirtualKey>& getVKs() const;
	const std::unordered_map<std::string, Action*>& getActions() const;
	const std::vector<Action*>& getActionsVector() const;
	int getKeyboardVK(SDLKey key) const;

	// The id a key is stored under in config.xml. The VK number is no good for
	// that: it is an index into virtualKeys and hangs off SDLK_LAST and off
	// which joysticks were plugged in at startup.
	const std::string& getVKId(int vk) const;
	int getVKFromId(const std::string& id) const;

	// What to show the player for a virtual key, localized and ready to draw.
	// An unknown one reads as unassigned rather than as nothing.
	std::string getVKDisplayName(int vk);

	// The keycaps an action is bound to, as <k>...</k> - both separated by a
	// slash, one on its own, and an unbound action as the word for it.
	std::string getBindingMarkup(const std::string& actionName);
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
	// Wait for a key press, to bind an action. A state and not a loop of its
	// own: in the browser only the return to the page fills the event queue.
	// While it runs, the keyboard belongs to it alone.
	enum
	{
		GRAB_WAITING   = -3,   // still running, nothing decided
		GRAB_TIMED_OUT = -2,   // nothing pressed: the binding stays as it was
		GRAB_NO_KEY    = -1    // Escape: means "no key" and clears the binding
	};

	// timeOutMS <= 0 waits without a deadline.
	void beginKeyGrab(int timeOutMS = 3000);
	bool isGrabbingKey() const;

	// Returns GRAB_WAITING while nothing is decided; otherwise the result
	// once, and that ends the key grab.
	int pollKeyGrab();
	void resetActions();

	// Reset only this one action to its default.
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
	// What the system speaks, boiled down to "de" or "en". Asked only where
	// config.xml names no language at all - see loadConfig().
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
	// Looks the text up, picks the language, and then expands whatever key
	// bindings it names - see expandBindings().
	std::string localizeString(const std::string& text);

	// Sounds that are to play quieter than their file is. The factor belongs
	// in the mix and not in the ogg: the .wav stays the unaltered source, and a
	// quietly encoded sound would have less level for the same computing cost.
	// Anything not listed in data/sounds.xml gets 1.0.
	void loadSoundVolumes(const std::string& filename);
	double getSoundVolumeFactor(const std::string& filename) const;
	std::string loadString(const std::string& id) const;

	AudioCapture* getAudioCapture();

	uint getTimePlayed() const { return timePlayed; }
	void saveTimePlayed();

	// A short message that slides in at the top of the picture. duration is
	// the hold time in seconds without the sliding in and out; 0 takes the
	// type's own value. An error plays teleport_failed.ogg unless suppressSound
	// stops it. The same message a second time only extends the first one's
	// hold time.
	enum ToastType
	{
		TOAST_OK = 0,
		TOAST_ERROR
	};

	void showToast(ToastType type, const std::string& text, double duration = 0.0, bool suppressSound = false);

private:
	Engine();
	~Engine();

	// The lookup and the language choice, without the binding expansion. It
	// recurses - a $ID resolves to a body that is looked up again, and a
	// missing German body falls back to the English one - and expanding on the
	// way out of every one of those would be work done several times over.
	std::string localizeStringRaw(const std::string& text);

	// Replaces %BINDING{$A_...} and %BINDING_OPTIONAL_RIGHT{$A_...} with the
	// keys the player has actually bound, so that no string in the game claims
	// a key that is no longer the one.
	std::string expandBindings(const std::string& text);

	// One toast in the stack. There are three sections: sliding in, standing,
	// sliding out. phaseTime restarts with each section.
	struct Toast
	{
		ToastType type;
		std::string text;
		int phase;          // 0 = in, 1 = standing, 2 = out
		uint phaseTime;     // ms in this section
		uint duration;      // ms for section 1
		double y;           // where the toast currently sits
		double targetY;     // where it wants to go
	};

	void setupCursor();
	SDL_Cursor* createCursor(int factor) const;
	void updateCursorSize();
	void fixWindowSize();
	void updateToasts();
	void renderToasts();

	// Hands out the places again: the newest at the very top, the older ones
	// below it. One that is already sliding out no longer counts.
	void reflowToasts();

	void updateKeyGrab();
	void syncActionDown(Action& action);

	// Clear the "just pressed" and "just released" edges of every action. The
	// held state stays.
	void clearActionEdges();

	// Put the last rendered frame on the screen once more: out of the
	// framebuffer, with bars and filter. No logic tick and nothing redrawn.
	void showLastFrame();

	// Sets window style and size without touching SDL's flags. The style
	// change itself is Win32; the size always goes through handleResize().
	void applyWindowStyle(bool wantFullScreen, const Vec2i& size);

	void rememberWindowPlacement();   // reads position/size off the window
	bool isWindowMaximized() const;   // maximized? then track nothing
	void restoreWindowPosition();     // puts them back at startup
#ifdef _WIN32
	void hookWindowProc();            // put our own window procedure in front
	void unhookWindowProc();          // and take it out again
#endif

	// The mapping of the currently effective filter, in both directions; the
	// coordinates run from -1 to 1 measured from the centre of the picture.
	// Only the CRT filter really warps anything, all the others return what
	// they were given.
	Vec2d warpToSource(const Vec2d& p) const;
	Vec2d warpToOutput(const Vec2d& p) const;

	// Oldest first, and that is the order they are drawn in: a dying toast
	// slides behind its younger neighbour and not over it.
	std::list<Toast> toasts;

	// The waiting state for a key press. grabOldState records which keys were
	// already down when it started - the wanted one is the key that goes down
	// *anew*.
	bool grabbingKey;
	int grabResult;
	uint grabDeadline;
	bool grabHasDeadline;
	std::vector<bool> grabOldState;

	bool initialized;
	bool fullScreen;
	int fullScreenOverride;    // -1 = nothing given on the command line
	bool splashSkipped;        // -nosplash
	bool frameBufferDisabled;  // -nofbo
	bool shadersDisabled;      // -noshader
	bool swallowedReturn;      // Alt+Return swallowed: the release too
	Vec2i windowedSize;        // size that leaving fullscreen falls back to
	Vec2i windowedPosition;    // ditto for the position
	bool  windowedPositionKnown;
	bool  maximized;           // was the window maximized on exit?
#ifdef _WIN32
	bool inSizeMove;           // user is holding the border or the title bar
#endif
	long savedWindowStyle;     // Win32: the style before fullscreen
	int savedWindowRect[4];    // Win32: x, y, w, h before fullscreen
	SDL_Surface* p_display;
	PFNGLBLENDFUNCSEPARATEEXTPROC glExtBlendFuncSeparate;
	ALCdevice* p_audioDevice;
	AudioCapture* p_audioCapture;
	ALCcontext* p_audioContext;
	uint logicRate;
	// The tables are indexed with the SDL keysym directly, and that counts
	// differently depending on the SDL header. Hence SDLK_LAST as the size, and
	// every index checked.
	static const int NUM_KEY_SLOTS = SDLK_LAST;   // 323 under SDL 1.2, 1536 with Emscripten's headers
	int keyData[NUM_KEY_SLOTS];
	// Is the key really under a finger right now? This lives here and not as a
	// bit in keyData, because keyData is zeroed from outside twice -
	// flushInput() clears it, and GS_Menu::onUpdate overwrites it every tick
	// with the recorded demo. A state that has to hold across several ticks
	// cannot live there.
	bool keyHeld[NUM_KEY_SLOTS];
	int buttonData[NUM_KEY_SLOTS];
	std::vector<SDL_Joystick*> joysticks;
	std::vector<VirtualKey> virtualKeys;
	std::unordered_map<std::string, Action*> actions;
	std::vector<Action*> actionsVector;
	// The key event and whether it is the repeat of a held key. An edit box
	// wants the repeat, a command does not - see Engine::getKeyEvent().
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
	// The mouse cursor at its design size. 0 is black, 1 white, -1 transparent.
	// Screenshots and videos draw it themselves and always from this: they
	// capture the 640x480 frame, in which it is exactly this size. What the
	// system draws stands beside it in two sizes.
	int cursorImage[16][16];
	SDL_Cursor* p_cursor1x;
	SDL_Cursor* p_cursor2x;
	int cursorScale;   // 1 or 2; 0 while none has been set yet
	uint oldImageID;
	uint newImageID;
	Crossfade* p_crossfade;
	double crossfadeTime;
	double crossfadeDuration;
	StreamedSound* p_currentMusic;
	std::string currentMusicFilename;
	std::unordered_map<std::string, uint> musicStoppedAt;
	// Framebuffer. frameTextureSize is a power of two, because WebGL 1 allows
	// NPOT textures only with restrictions; the bottom left corner is used.
	uint frameBufferID;
	uint frameTextureID;
	uint frameDepthStencilID;
	Vec2i frameTextureSize;
	uint renderTargetID;       // the FBO to draw into, with no fixed texture
	bool renderTargetScissor;  // was the scissor box on when this began?
	// The pool of textures to draw into, see acquireOffscreenTexture().
	struct OffscreenTexture
	{
		uint id;
		Vec2i size;
		bool lent;
	};
	std::vector<OffscreenTexture> offscreenTextures;
	bool useFrameBuffer;
	// The four filters. upscalers owns them and holds the options dialog's
	// order; the four pointers beside it are the shortcut to them.
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
	// Focus gained or lost, from whichever event.
	void handleAppFocus(bool gained);

	// Does the window have the focus? A member and not a loop variable,
	// because emscripten_set_main_loop calls once per frame and nothing may
	// live on the stack between them - and because the test hook reports it.
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
	std::unordered_map<std::string, double> soundVolumes;
	uint timePlayed;
	bool doScreenshot;
};

#endif