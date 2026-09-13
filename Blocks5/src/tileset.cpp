#include "pch.h"
#include "tileset.h"
#include "filesystem.h"
#include "texture.h"

TileSet::TileSet(const std::string& filename) : Resource(filename)
{
	p_texture = 0;

	// Before reload(), which starts every tile from badTile.
	badTile.position = Vec2i(-1, -1);
	badTile.type = -1;
	badTile.destroyTime = 0;

	reload();
}

TileSet::~TileSet()
{
	cleanUp();
}

void TileSet::reload()
{
	// load the XML document
	std::string text = FileSystem::inst().readStringFromFile(filename);
	TiXmlDocument doc;
	doc.Parse(text.c_str());
	if(doc.ErrorId())
	{
		printfLog("+ ERROR: Could not parse tileset XML file \"%s\" (Error: %d).\n",
				  filename.c_str(),
				  doc.ErrorId());
		error = 1;
		return;
	}

	TiXmlHandle docHandle(&doc);
	TiXmlHandle tileSetHandle = docHandle.FirstChildElement("TileSet");
	TiXmlElement* p_tileSetElement = tileSetHandle.Element();
	if(!p_tileSetElement)
	{
		printfLog("+ ERROR: Tileset XML file \"%s\" has no <TileSet> element.\n",
				  filename.c_str());
		error = 3;
		return;
	}

	// read the image filename and the tile size
	const char* p_imageFilename = p_tileSetElement->Attribute("image");
	if(!p_imageFilename)
	{
		printfLog("+ ERROR: Tileset \"%s\" names no image.\n", filename.c_str());
		error = 4;
		return;
	}

	// The size is fixed; the file is only held to its word. A missing entry
	// counts as correct, because TiXmlElement::Attribute leaves the value
	// untouched when the attribute is absent.
	int fileTileWidth = TILE_SIZE, fileTileHeight = TILE_SIZE;
	p_tileSetElement->Attribute("tileWidth", &fileTileWidth);
	p_tileSetElement->Attribute("tileHeight", &fileTileHeight);
	if(fileTileWidth != TILE_SIZE || fileTileHeight != TILE_SIZE)
	{
		printfLog("+ ERROR: Tileset \"%s\" has %dx%d tiles; only %dx%d is supported.\n",
				  filename.c_str(),
				  fileTileWidth, fileTileHeight,
				  TILE_SIZE, TILE_SIZE);
		error = 5;
		return;
	}

	// load the texture
	std::string dir = FileSystem::inst().getPathDirectory(filename);
	std::string imageFilename = dir + (dir.empty() ? "" : "/") + std::string(p_imageFilename);
	p_texture = Manager<Texture>::inst().request(imageFilename);
	if(!p_texture)
	{
		printfLog("+ ERROR: Could not load tileset texture \"%s\" for tileset \"%s\".\n",
				  p_imageFilename,
				  filename.c_str());
		error = 2;
		return;
	}

	p_texture->keepInMemory();

	maxTileID = 0;

	// process all child elements
	TiXmlElement* p_tileElement = p_tileSetElement->FirstChildElement("Tile");
	while(p_tileElement)
	{
		TileInfo info = badTile;

		// The id is the attribute's first character taken as a byte, which is
		// the whole id space: a level stores one id per character of its <Row>
		// strings, and tiles[] is sized 256 for exactly that. Through unsigned
		// char, or a tile named with a non-ASCII character would sign-extend
		// to a huge index and be written far outside the array. A skin can
		// come from a stranger, so a <Tile> carrying no id at all is a broken
		// file rather than a null pointer to walk into.
		const char* p_id = p_tileElement->Attribute("id");
		if(!p_id || !*p_id)
		{
			printfLog("+ ERROR: Tileset \"%s\" has a <Tile> without an id.\n",
					  filename.c_str());
			error = 6;
			return;
		}

		const uint id = static_cast<unsigned char>(*p_id);
		maxTileID = max(maxTileID, id);

		// read the position
		p_tileElement->Attribute("x", &info.position.x);
		p_tileElement->Attribute("y", &info.position.y);

		// read the type
		p_tileElement->Attribute("type", &info.type);

		if(info.type == 2)
		{
			// read the destroy time
			p_tileElement->Attribute("destroyTime", &info.destroyTime);

			// Where the debris takes its colour from: the tile's image.
			info.sprites.setTexture(p_texture);
			info.sprites.add(info.position);
		}

		// record the tile type
		tiles[id] = info;

		p_tileElement = p_tileElement->NextSiblingElement("Tile");
	}
}

void TileSet::cleanUp()
{
	if(p_texture)
	{
		// release the texture
		p_texture->release();
		p_texture = 0;
	}

	// reset every tile
	for(int i = 0; i < 256; i++) tiles[i] = badTile;
}

void TileSet::writeTile(uint id,
						const Vec2f& position,
						std::vector<QuadVertex>& out) const
{
	if(id == 0) return;

	const TileInfo& tile = getTileInfo(id);
	if(tile.type == -1) return;

	// TILE_SIZE is the tile's edge in the picture and in the texture alike,
	// which is why one constant does for both here.
	const double s = TILE_SIZE;
	const double x = position.x, y = position.y;
	const double u = tile.position.x, v = tile.position.y;

	out.push_back(QuadVertex(x,     y,     u,     v));
	out.push_back(QuadVertex(x + s, y,     u + s, v));
	out.push_back(QuadVertex(x + s, y + s, u + s, v + s));
	out.push_back(QuadVertex(x,     y + s, u,     v + s));
}

void TileSet::drawVertices(const QuadVertex* p_vertices,
						   uint count) const
{
	// The bind and the switch back off happen even for a layer with no tiles in
	// it: the state every later draw inherits must not depend on whether a
	// layer happened to be empty.
	p_texture->bind();
	drawQuadArray(p_vertices, count);
	GL::setTexturing(false);
}

Texture* TileSet::getTexture()
{
	return p_texture;
}

const TileSet::TileInfo& TileSet::getTileInfo(uint id) const
{
	if(id >= 256) return badTile;
	else return tiles[id];
}

uint TileSet::getMaxTileID() const
{
	return maxTileID;
}