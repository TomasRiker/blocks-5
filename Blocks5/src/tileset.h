#ifndef _TILESET_H
#define _TILESET_H

/*** Class for tiles ***/

#include "resource.h"
#include "sprite.h"
// For QuadVertex, which writeTile() fills and drawVertices() draws.
#include "renderer.h"

class Texture;

class TileSet : public Resource<TileSet>
{
	friend class Manager<TileSet>;

public:
	// A tile is 16x16, always: the editor knows nothing else, every tileset
	// in the tree (the four shipped skins and the third-party lego skin) names
	// exactly that size, and reload() rejects a file that claims otherwise.
	static const int TILE_SIZE = 16;

	struct TileInfo
	{
		Vec2i position;
		int type;
		int destroyTime;

		// The tile's image, which debris takes its colour from: one sprite,
		// unrotated and untinted. Empty for a tile that cannot be destroyed,
		// which then leaves no debris.
		Sprites sprites;
	};

	TileSet(const std::string& filename, int options);
	~TileSet();

	void reload();
	void cleanUp();

	void writeTile(uint id, const Vec2f& position, std::vector<QuadVertex>& out) const;
	void drawVertices(const QuadVertex* p_vertices, uint count, const Vec4f& color) const;

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