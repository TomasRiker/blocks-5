#include "pch.h"
#include "tileset.h"
#include "filesystem.h"
#include "texture.h"
#include "debriscolordb.h"

TileSet::TileSet(const std::string& filename) : Resource(filename)
{
	p_texture = 0;

	reload();

	badTile.position = Vec2i(-1, -1);
	badTile.type = -1;
	badTile.destroyTime = 0;
	badTile.debrisColor = Vec4d(0.0, 0.0, 0.0, 0.0);
}

TileSet::~TileSet()
{
	cleanUp();
}

void TileSet::reload()
{
	// XML-Dokument laden
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

	// Dateiname des Bilds und Groesse der Tiles lesen
	const char* p_imageFilename = p_tileSetElement->Attribute("image");
	if(!p_imageFilename)
	{
		printfLog("+ ERROR: Tileset \"%s\" names no image.\n", filename.c_str());
		error = 4;
		return;
	}

	// Die Groesse steht fest; die Datei wird nur beim Wort genommen. Fehlende
	// Angaben gelten als richtig, weil TiXmlElement::Attribute den Wert
	// unberuehrt laesst, wenn es das Attribut nicht gibt.
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

	// Textur laden
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

	// alle Kind-Elemente verarbeiten
	TiXmlElement* p_tileElement = p_tileSetElement->FirstChildElement("Tile");
	while(p_tileElement)
	{
		TileInfo info = badTile;

		// ID lesen
		uint id = static_cast<uint>(p_tileElement->Attribute("id")[0]);
		maxTileID = max(maxTileID, id);

		// Position lesen
		p_tileElement->Attribute("x", &info.position.x);
		p_tileElement->Attribute("y", &info.position.y);

		// Typ lesen
		p_tileElement->Attribute("type", &info.type);

		if(info.type == 2)
		{
			// Zerstoerzeit lesen
			p_tileElement->Attribute("destroyTime", &info.destroyTime);

			// Truemmerfarbe berechnen
			info.debrisColor = DebrisColorDB::inst().getDebrisColor(p_texture, info.position);
		}

		// Tile-Typ eintragen
		tiles[id] = info;

		p_tileElement = p_tileElement->NextSiblingElement("Tile");
	}
}

void TileSet::cleanUp()
{
	if(p_texture)
	{
		// Textur loeschen
		p_texture->release();
		p_texture = 0;
	}

	// alle Tiles zuruecksetzen
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