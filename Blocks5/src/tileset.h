#ifndef _TILESET_H
#define _TILESET_H

/*** Klasse für Tiles ***/

#include "resource.h"

class Texture;

class TileSet : public Resource<TileSet>
{
	friend class Manager<TileSet>;

public:
	struct TileInfo
	{
		Vec2i position;
		int type;
		int destroyTime;
		Vec4d debrisColor;
	};

	TileSet(const std::string& filename);
	~TileSet();

	void reload();
	void cleanUp();

	void beginRender();
	void endRender();
	void renderTile(uint id, const Vec2d& position);

	Texture* getTexture();
	const Vec2i& getTileSize() const;
	const TileInfo& getTileInfo(uint id) const;
	uint getMaxTileID() const;

private:
	static bool forceReload() { return false; }

	Texture* p_texture;
	Vec2i tileSize;
	TileInfo tiles[256];
	uint maxTileID;
	TileInfo badTile;
};

#endif