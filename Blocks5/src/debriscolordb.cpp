#include "pch.h"
#include "debriscolordb.h"
#include "texture.h"
#include "engine.h"

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

	// TODO: Trümmer nicht einfarbig, sondern zufällig machen (z.B. zufällig aus den 10 häufigsten Farben auswählen)

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
		for(int x = 0; x < 16; x++)
		{
			for(int y = 0; y < 16; y++)
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