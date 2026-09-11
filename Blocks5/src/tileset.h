#ifndef _TILESET_H
#define _TILESET_H

/*** Class for tiles ***/

#include "resource.h"
#include "sprite.h"

class Texture;

class TileSet : public Resource<TileSet>
{
	friend class Manager<TileSet>;

public:
	// A tile is 16x16, always. The editor knows nothing else, all nine
	// tileset.xml in the tree - the four shipped skins, their archives and the
	// third-party lego skin - name exactly that size, and reload() rejects a
	// file that claims otherwise. A single number because tiles are square;
	// that way the value stands here in the header and needs no definition in
	// the .cpp.
	static const int TILE_SIZE = 16;

	struct TileInfo
	{
		Vec2i position;
		int type;
		int destroyTime;

		// The tile's image, which the debris takes its colour from. Exactly one
		// sprite, unrotated and untinted - there is nothing composite about a
		// tile. For a tile that is not destroyable at all the list stays empty,
		// and then no debris appears either.
		Sprites sprites;
	};

	TileSet(const std::string& filename);
	~TileSet();

	void reload();
	void cleanUp();

	// One corner of a tile. Position and texture coordinate and nothing else:
	// the grid carries no per-vertex colour, which is exactly what lets one
	// built array serve all three passes Level::render makes over a layer -
	// two shadow samples and the picture - under nothing but a different
	// glColor and a different translate. Float and not int, because GL_INT is
	// not a valid vertex attribute type in WebGL/GLES2, and a texel and a whole
	// pixel are both well inside float's exact range.
	struct Vertex
	{
		Vec2f position;
		Vec2f uv;
	};

	void writeTile(uint id, const Vec2f& position, std::vector<Vertex>& out) const;
	void drawVertices(const Vertex* p_vertices, uint count) const;

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