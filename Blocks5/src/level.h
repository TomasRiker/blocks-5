#ifndef _LEVEL_H
#define _LEVEL_H

#include "lightning.h"

/*** Klasse für einen Level ***/

class Texture;
class Font;
class TileSet;
class Presets;
class Exit;
class ParticleSystem;
class Elevator;
class Rail;
class SoundInstance;
class Electronics;

class Level
{
	friend class Object;
	friend class Player;
	friend class Exit;

public:
	Level();
	~Level();

	void clear();
	bool load(const std::string& filename, bool dontLoadSkin = false);
	bool load(TiXmlDocument* p_doc, bool dontReallyLoad = false);
	bool save(const std::string& filename);
	TiXmlDocument* save();

	void render();
	void update();
	void renderTiles(int layer, const Vec2i& offset, const Vec4d& color);
	void renderObjects(int layer, const Vec2i& offset, const Vec4d& color, bool shadow);
	void sortObjects();
	void renderShine(double intensity, double size);
	bool isFreeAt(const Vec2i& position, int* p_tileTypeOut = 0);
	bool isFreeAt2(const Vec2i& positionInPixels, Object* p_except, Object** pp_objectOut, Vec2i* p_tileOut, double radiusSq = 74.0);
	Object* getFrontObjectAt(const Vec2i& position);
	Object* getBackObjectAt(const Vec2i& position);
	Elevator* getElevatorAt(const Vec2i& position);
	Rail* getRailAt(const Vec2i& position);
	Player* getPlayerAt(const Vec2i& position);
	std::vector<Object*> getObjectsAt(const Vec2i& position);
	std::vector<Object*> getObjectsAt2(const Vec2i& position, double radiusSq = 74.0);
	const std::vector<Object*>& getAllObjectsAt(const Vec2i& position);
	void addObject(Object* p_object);
	void removeObject(Object* p_object);
	void addNewObjects();
	void removeOldObjects();
	void clean();
	void hashObject(Object* p_obj);
	void unhashObject(Object* p_obj);

	void setAIFlag(const Vec2i& where, uint flag);
	void unsetAIFlag(const Vec2i& where, uint flag);
	void clearAIFlags(const Vec2i& where);
	uint getAIFlags(const Vec2i& where) const;
	uint getAITrace(const Vec2i& where) const;
	void setAITrace(const Vec2i& where, uint value);

	bool isValidPosition(const Vec2i& position) const;

	uint getTileAt(int layer, const Vec2i& position) const;
	void setTileAt(int layer, const Vec2i& position, uint tile);
	uint getTileDestroyTimeAt(int layer, const Vec2i& position) const;
	void setTileDestroyTimeAt(int layer, const Vec2i& position, uint destroyTime);
	bool clearPosition(const Vec2i& position, const std::string& except = "");

	void turnArrows();
	bool changeBarrages(uint color);
	int changeBarrages2(uint color, bool up);
	int fireCannons(uint color);
	int rotateCannons(uint color);

	const std::string& getTitle() const;
	void setTitle(const std::string& title);
	std::string getSkin(uint index) const;
	bool setSkin(uint index, const std::string& skin);
	const Vec2i& getSize() const;
	Vec2i getSizeInPixels() const;
	void setSize(const Vec2i& size);
	int getNumLayers() const;
	bool isInEditor() const;
	void setInEditor(bool inEditor);
	bool isInCat() const;
	void setInCat(bool inCat);
	bool isInPreview() const;
	void setInPreview(bool inPreview);
	bool isInMenu() const;
	void setInMenu(bool inMenu);
	TileSet* getTileSet();
	void setTileSet(TileSet* p_tileSet);
	ParticleSystem* getParticleSystem();
	ParticleSystem* getFireParticleSystem();
	Texture* getSprites();
	Texture** getLava();
	Texture* getBackground();
	Texture* getHint();
	Font* getHintFont();
	Presets* getPresets();
	const std::vector<Object*>& getObjects() const;
	Player* getActivePlayer();
	void switchToNextPlayer();
	Exit* getExit();
	uint getNumDiamondsNeeded() const;
	void setNumDiamondsNeeded(uint numDiamondsNeeded);
	void setNumDiamondsCollected(uint numDiamondsCollected);
	uint getNumDiamondsCollected() const;
	bool isElectricityOn() const;
	void setElectricityOn(bool electricityOn);
	bool isNightVision() const;
	void setNightVision(bool nightVision);
	bool isRaining() const;
	void setRaining(bool raining);
	bool isCloudy() const;
	void setCloudy(bool cloudy);
	bool isSnowing() const;
	void setSnowing(bool snowing);
	bool isThunderstorm() const;
	void setThunderstorm(bool thunderstorm);
	const Vec3i& getLightColor() const;
	void setLightColor(const Vec3i& lightColor);
	const std::string& getMusicFilename() const;
	void setMusicFilename(const std::string& musicFilename);
	void addCameraShake(double value);
	void addFlash(double value);
	void addToxic(double value);
	void invalidate();

	std::string getSkinFilename(uint index);
	static std::string getAlternative(const std::string& filename, const std::string& dir1, const std::string& dir2);
	void loadSkin(bool forceReload = false);

	int counter;
	int time;
	bool finished;
	std::set<std::string> skinsMissing;
	std::set<Electronics*> allElectronics;

	enum Skin
	{
		SKIN_TILESET = 0,
		SKIN_SPRITES,
		SKIN_PARTICLES,
		SKIN_BACKGROUND,
		SKIN_HINT,
		SKIN_HINTFONT,
		SKIN_NOISE,
		SKIN_SHINE,
		SKIN_RAIN,
		SKIN_CLOUDS,
		SKIN_SNOW,

		SKIN_MAX
	};

private:
	std::string title;
	std::string skin[SKIN_MAX];
	std::string requestedSkin[SKIN_MAX];
	std::string filename;
	Vec2i size;
	int numLayers;
	bool inEditor;
	bool inCat;
	bool inPreview;
	bool inMenu;
	TileSet* p_tileSet;
	uint* p_tiles;
	uint* p_aiFlags;
	Texture* p_sprites;
	Texture* p_lava[2];
	Texture* p_noise;
	Texture* p_shine;
	Texture* p_rain;
	Texture* p_clouds;
	Texture* p_snow;
	Texture* p_background;
	Texture* p_hint;
	Font* p_hintFont;
	uint layerListBase;
	uint layerDirty;
	Presets* p_presets;
	std::vector<Object*> emptyObjectList;
	std::vector<Object*>* p_objectsAt;
	std::vector<Object*> objects;
	std::vector<Object*> objectsToAdd;
	std::vector<Object*> objectsToRemove;
	Player* p_activePlayer;
	Exit* p_exit;
	ParticleSystem* p_particleSystem;
	ParticleSystem* p_fireParticleSystem;
	ParticleSystem* p_rainParticleSystem;
	Texture* p_particleSprites;
	uint numDiamondsNeeded;
	uint numDiamondsCollected;
	bool electricityOn;
	bool nightVision;
	bool raining;
	bool cloudy;
	bool snowing;
	bool thunderstorm;
	double cameraShake;
	double flash;
	double flashJitter;
	double actualFlash;
	Vec3i lightColor;
	std::string musicFilename;
	uint bufferID;
	double toxic;
	uint lightningCounter;
	Lightning lightning;

	static SoundInstance* p_rainSoundInst;
	static SoundInstance* p_thunderstormSoundInst;
	static bool rainSoundOn;
	static bool thunderstormSoundOn;

	void renderToxicEffect();
};

#endif