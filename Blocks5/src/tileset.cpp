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

	// Dateiname des Bilds und Größe der Tiles lesen
	const char* p_imageFilename = p_tileSetElement->Attribute("image");
	p_tileSetElement->Attribute("tileWidth", &tileSize.x);
	p_tileSetElement->Attribute("tileHeight", &tileSize.y);

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
			// Zerstörzeit lesen
			p_tileElement->Attribute("destroyTime", &info.destroyTime);

			// Trümmerfarbe berechnen
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
		// Textur löschen
		p_texture->release();
		p_texture = 0;
	}

	// alle Tiles zurücksetzen
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

	glTexCoord2i(tile.position.x + tileSize.x, tile.position.y);
	glVertex2d(position.x + tileSize.x, position.y);

	glTexCoord2i(tile.position.x + tileSize.x, tile.position.y + tileSize.y);
	glVertex2d(position.x + tileSize.x, position.y + tileSize.y);

	glTexCoord2i(tile.position.x, tile.position.y + tileSize.y);
	glVertex2d(position.x, position.y + tileSize.y);
}

Texture* TileSet::getTexture()
{
	return p_texture;
}

const Vec2i& TileSet::getTileSize() const
{
	return tileSize;
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