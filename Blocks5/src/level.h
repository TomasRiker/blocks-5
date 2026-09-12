#ifndef _LEVEL_H
#define _LEVEL_H

#include "lightning.h"
// For QuadVertex, which the per-layer arrays below are made of. The forward
// declaration of TileSet a few lines down is enough for the rest.
#include "quadarray.h"
#include "renderlayer.h"

/*** Class for a level ***/

class Object;
class Player;
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
	// Size and layer count are the same for every level and not a property of
	// the individual instance: the editor allows nothing else, and all 220
	// shipped and third-party level files in the tree name exactly these
	// values. The level file still writes them out, to keep it readable on
	// its own and openable by older versions of the game - but on load they
	// are only checked, no longer adopted.
	//
	// Separate ints rather than a Vec2i, to let the value stand here in the
	// header: an integral static const member may be initialised inside the
	// class and is therefore a constant expression in every translation unit.
	// A Vec2i needs a definition in a .cpp and is, to every other file, only
	// a symbol that has to be read.
	static const int WIDTH = 40;
	static const int HEIGHT = 25;
	static const int NUM_LAYERS = 2;

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
	void renderObjects(RenderLayer layer, const Vec2i& offset, const Vec4d& color, bool shadow);
	void sortObjects();
	// The offset is added inside the caller's own matrix, which is why it is not
	// called a position: it is where the glow sits relative to the cell the
	// object is already drawn in. A laser wants one per four pixels of beam and
	// a light barrier one per pixel, and a matrix bracket each would be the
	// most expensive thing in the frame.
	void renderShine(double intensity, double size, const Vec2d& offset = Vec2d(0.0));
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
	bool isValidLayer(int layer) const;

	uint getTileAt(int layer, const Vec2i& position) const;
	void setTileAt(int layer, const Vec2i& position, uint tile);
	uint getTileDestroyTimeAt(int layer, const Vec2i& position) const;
	void setTileDestroyTimeAt(int layer, const Vec2i& position, uint destroyTime);
	bool clearPosition(const Vec2i& position, const std::string& except = "");

	void turnArrows();
	bool changeBarrages(uint color);
	bool changeBarrages2(uint color, bool up);
	int fireCannons(uint color);
	int rotateCannons(uint color);

	const std::string& getTitle() const;
	void setTitle(const std::string& title);
	std::string getSkin(uint index) const;
	bool setSkin(uint index, const std::string& skin);
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
	Texture* getSpritesTexture();
	Texture** getLava();
	Texture* getBackground();
	Texture* getHint();
	Font* getHintFont();

	// Should the hint note roll up? That is decided by the artwork and not by
	// the level: a sheet of paper rolls up, a tablet does not. See loadSkin().
	bool isHintScroll() const;
	Presets* getPresets();
	const std::vector<Object*>& getObjects() const;
	Player* getActivePlayer();

	// Close whatever the player is currently being shown on their own field -
	// today that is the hint note. Returns false when nothing needed closing;
	// then the key belongs to its real recipient.
	bool dismissDisplay();

	void switchToNextPlayer();
	Exit* getExit();
	uint getNumDiamondsNeeded() const;
	void setNumDiamondsNeeded(uint numDiamondsNeeded);
	void setNumDiamondsCollected(uint numDiamondsCollected);

	// The two icons at the bottom left light up when the player collects
	// something - the same gesture with which a switch answers a press. The
	// index is the inventory's: 0 the bomb, 1 the diamond.
	void flashHudIcon(uint index);
	double getHudIconFlash(uint index) const;
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
	void allocateTiles();
	bool loadErrorLevel();
	void loadSkin(bool forceReload = false);

	int counter;
	// Milliseconds since the level was loaded, 20 to a tick. Signed, so it is
	// undefined after 24.9 days in one level - where GS_Menu's and Engine's
	// own counters are uint and merely wrap, at 49.7. Neither is reachable by
	// playing; both need a machine left on one screen for weeks. Making this
	// uint too would turn the undefined case into a defined one, and is worth
	// folding into the next edit here rather than doing on its own.
	int time;
	bool finished;
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
	bool hintScroll;
	uint layerDirty;
	// The grid as vertices, one array per layer, built only when layerDirty
	// says the layer changed. They hold the layer's own coordinates, so the
	// offset and the colour of a pass stay outside and the two shadow samples
	// and the picture are three draws of one array. A full layer is 1000 tiles
	// and so 64 KB; a palette level is nearly empty and costs nearly nothing,
	// which matters because the editor holds six Levels at once.
	std::vector<QuadVertex> tileVertices[NUM_LAYERS];
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
	double hudIconFlash[2];
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