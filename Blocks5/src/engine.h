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

	bool down;
};

class Engine : public Singleton<Engine>
{
	friend class Singleton<Engine>;

public:
	bool init(const std::string& windowCaption, const std::string& windowIconFilename, uint width, uint height, bool fullScreen, bool useHQ2X);
	void exit();
	void mainLoop();
	void render();
	void update();
	void updateSounds();

	std::string getBestOpenALDevice();
	std::string getBestOpenALCaptureDevice();
	void drawOverlays();
	void upscaleFrame();
	void screenshot();
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
	const stdext::hash_map<std::string, Action*>& getActions() const;
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

	ALCdevice* getOpenALCaptureDevice();

	uint getTimePlayed() const { return timePlayed; }

private:
	Engine();
	~Engine();

	void setupCursor();

	bool initialized;
	bool fullScreen;
	bool useHQ2X;
	SDL_Surface* p_display;
	PFNGLBLENDFUNCSEPARATEEXTPROC glExtBlendFuncSeparate;
	ALCdevice* p_audioDevice;
	ALCdevice* p_audioCaptureDevice;
	ALCcontext* p_audioContext;
	uint logicRate;
	bool modal;
	int keyData[512];
	int buttonData[512];
	std::vector<SDL_Joystick*> joysticks;
	std::vector<VirtualKey> virtualKeys;
	stdext::hash_map<std::string, Action*> actions;
	std::vector<Action*> actionsVector;
	std::queue<SDL_KeyboardEvent> keyEventQueue;
	stdext::hash_map<std::string, GameState*> gameStates;
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
	unsigned char* p_hq2xIn;
	unsigned char* p_hq2xOut;
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

	stdext::hash_map<std::string, std::string> stringDB;
	uint timePlayed;
	bool doScreenshot;
};

#endif