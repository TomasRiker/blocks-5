#include "pch.h"
#include "debriscolordb.h"
#include "texture.h"
#include "engine.h"
#include "tileset.h"
#include "object.h"

// Die Zelle, aus der gezogen wird, ist bei Kacheln und bei Objektbildern
// dieselbe. Waeren die beiden je verschieden, muesste sample() die Groesse
// mitfuehren statt sie zu kennen.
static_assert(Object::SPRITE_SIZE == TileSet::TILE_SIZE,
			  "debris sampling assumes tiles and object sprites share a cell size");

DebrisSource::DebrisSource()
{
	p_texture = 0;
	positionOnTexture = Vec2i(0, 0);
	average = Vec4d(0.0, 0.0, 0.0, 0.0);
}

void DebrisSource::setTexture(Texture* p_texture,
							  const Vec2i& positionOnTexture)
{
	this->p_texture = p_texture;
	this->positionOnTexture = positionOnTexture;
	average = DebrisColorDB::inst().getDebrisColor(p_texture, positionOnTexture);
}

void DebrisSource::setColor(const Vec4d& color)
{
	p_texture = 0;
	positionOnTexture = Vec2i(0, 0);
	average = color;
}

bool DebrisSource::sample(Vec4d* p_colorOut,
						  Vec2i* p_offsetOut) const
{
	// Ohne Textur - oder wenn die Pixel nicht mehr im Speicher liegen, dann
	// gaebe getPixel() fuer alles durchsichtiges Schwarz und es entstuende gar
	// kein Partikel mehr - bleibt es beim Mittelwert von frueher.
	if(!p_texture || !p_texture->hasPixels())
	{
		*p_colorOut = average;
		*p_offsetOut = Vec2i(TileSet::TILE_SIZE / 2, TileSet::TILE_SIZE / 2);
		return true;
	}

	// Eine einzige Zufallszahl reicht. randomInt() ist mt.randInt(0x7FFFFFFF),
	// und MTRand::randInt(n) maskiert bei einer Maske aus lauter Einsen nur -
	// die Verwerfungsschleife dort laeuft nie. Es sind also 31 unverbrauchte
	// Mersenne-Twister-Bits, und die sind bis zur 32. Stelle gleichverteilt.
	// Vier davon fuer x, vier fuer y, acht fuer die Schwelle.
	const uint r = static_cast<uint>(randomInt());
	const int x = static_cast<int>(r & 15);
	const int y = static_cast<int>((r >> 4) & 15);
	const uint threshold = (r >> 8) & 255;

	const Vec4d pixel = p_texture->getPixel(positionOnTexture + Vec2i(x, y));

	// Annehmen mit der Wahrscheinlichkeit der eigenen Deckkraft. Ein voll
	// deckendes Pixel faellt damit in einem von 256 Faellen durch; das ist
	// nicht zu sehen und spart den Sonderfall.
	if(static_cast<uint>(pixel.a * 255.0) <= threshold) return false;

	*p_colorOut = Vec4d(pixel.r, pixel.g, pixel.b, average.a);
	*p_offsetOut = Vec2i(x, y);
	return true;
}

DebrisColorDB::DebrisColorDB()
{
}

DebrisColorDB::~DebrisColorDB()
{
}

Vec4d DebrisColorDB::getDebrisColor(Texture* p_texture,
									const Vec2i& positionOnTexture)
{
	const std::string& filename = p_texture->getFilename();

	dbKey key;
	key.first = filename;
	key.second = positionOnTexture;

	// Schon berechnet?
	dbMap::const_iterator i = db.find(key);
	if(i != db.end())
	{
		// Ergebnis liefern
		return i->second;
	}
	else
	{
		// berechnen
		Vec4d sum(0.0);
		for(int x = 0; x < TileSet::TILE_SIZE; x++)
		{
			for(int y = 0; y < TileSet::TILE_SIZE; y++)
			{
				Vec4d pixel = p_texture->getPixel(positionOnTexture + Vec2i(x, y));
				sum.r += pixel.r * pixel.a;
				sum.g += pixel.g * pixel.a;
				sum.b += pixel.b * pixel.a;
				sum.a += pixel.a;
			}
		}

		if(sum.a != 0.0)
		{
			sum /= sum.a;
			sum.a = 0.25;
		}
		else sum = Vec4d(0.0);

		// eintragen
		db[key] = sum;

		return sum;
	}
}