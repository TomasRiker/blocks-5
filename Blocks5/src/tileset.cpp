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

		// read the id
		uint id = static_cast<uint>(p_tileElement->Attribute("id")[0]);
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

void TileSet::beginRender()
{
	p_texture->bind();
	glBegin(GL_QUADS);
}

void TileSet::endRender()
{
	glEnd();
	p_texture->unbind();
}

void TileSet::renderTile(uint id,
						 const Vec2d& position)
{
	if(id == 0) return;

	const TileInfo& tile = getTileInfo(id);
	if(tile.type == -1) return;

	glTexCoord2i(tile.position.x, tile.position.y);
	glVertex2d(position.x, position.y);

	glTexCoord2i(tile.position.x + TILE_SIZE, tile.position.y);
	glVertex2d(position.x + TILE_SIZE, position.y);

	glTexCoord2i(tile.position.x + TILE_SIZE, tile.position.y + TILE_SIZE);
	glVertex2d(position.x + TILE_SIZE, position.y + TILE_SIZE);

	glTexCoord2i(tile.position.x, tile.position.y + TILE_SIZE);
	glVertex2d(position.x, position.y + TILE_SIZE);
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