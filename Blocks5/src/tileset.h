#ifndef _TILESET_H
#define _TILESET_H

/*** Klasse fuer Tiles ***/

#include "resource.h"
#include "debriscolordb.h"

class Texture;

class TileSet : public Resource<TileSet>
{
	friend class Manager<TileSet>;

public:
	// Ein Tile ist 16x16, immer. Der Editor kennt nichts anderes, alle neun
	// tileset.xml im Baum - die vier mitgelieferten Skins, ihre Archive und der
	// fremde lego-Skin - nennen genau diese Groesse, und reload() weist eine
	// Datei ab, die etwas anderes behauptet. Eine einzelne Zahl, weil Tiles
	// quadratisch sind; so steht der Wert hier im Kopf und braucht keine
	// Definition in der .cpp.
	static const int TILE_SIZE = 16;

	struct TileInfo
	{
		Vec2i position;
		int type;
		int destroyTime;
		DebrisSource debris;
	};

	TileSet(const std::string& filename);
	~TileSet();

	void reload();
	void cleanUp();

	void beginRender();
	void endRender();
	void renderTile(uint id, const Vec2d& position);

	Texture* getTexture();
	const TileInfo& getTileInfo(uint id) const;
	uint getMaxTileID() const;

private:
	static bool forceReload() { return false; }

	Texture* p_texture;
	TileInfo tiles[256];
	uint maxTileID;
	TileInfo badTile;
};

#endif