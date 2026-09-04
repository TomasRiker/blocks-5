#include "pch.h"
#include "sprite.h"
#include "texture.h"
#include "tileset.h"

// Die Zelle, aus der gezogen wird, ist bei Kacheln und bei Objektbildern
// dieselbe. Waeren die beiden je verschieden, muesste getTryCount() eine
// andere Bezugsflaeche nehmen.
static_assert(Sprite::SIZE == TileSet::TILE_SIZE,
			  "debris sampling assumes tiles and object sprites share a cell size");

Sprite::Sprite()
{
	positionOnTexture = Vec2i(0, 0);
	size = Vec2i(SIZE, SIZE);
	offset = Vec2i(0, 0);
	color = Vec4d(1.0, 1.0, 1.0, 1.0);
	mirrorX = false;
	rotation = 0.0;
}

namespace
{
	const double p_degToRad = 3.1415926535897932384626433832795 / 180.0;

	// Womit die Huelle beschnitten wird, bevor floor() und ceil() darauf
	// losgehen. cos(90 Grad) ist 6.1e-17 und nicht 0, die linke Kante einer
	// Vierteldrehung kommt als -8.9e-16 heraus, und die Zelle waere ohne Not
	// 17x17 gross - getTryCount() rechnet mit der Flaeche und wuerfe 13% mehr
	// Truemmer als dasselbe Bild ungedreht.
	const double p_footprintEpsilon = 1.0e-6;

	// Die Stelle in Objektkoordinaten in die Zelle zuruecksuchen, aus der
	// renderSprite sie geholt hat: erst der Versatz, dann die Gegendrehung,
	// dann die Spiegelung, die ihre eigene Umkehrung ist. Weil es dieselbe
	// Matrix im selben Bezugssystem ist, stimmt das Ergebnis unabhaengig davon,
	// wohin die y-Achse zeigt. Gerechnet wird mit Pixelmitten und floor() -
	// sonst ginge bei jeder Drehung eine halbe Pixelreihe verloren.
	bool mapToTexel(const Sprite& sprite,
					const Vec2i& point,
					Vec2i* p_texelOut)
	{
		const Vec2d half(0.5 * sprite.size.x, 0.5 * sprite.size.y);
		const Vec2d d(point.x + 0.5 - sprite.offset.x - half.x,
					  point.y + 0.5 - sprite.offset.y - half.y);

		Vec2d m(d);
		if(sprite.rotation != 0.0)
		{
			const double a = sprite.rotation * p_degToRad;
			const double c = cos(a);
			const double s = sin(a);
			m.x =  c * d.x + s * d.y;
			m.y = -s * d.x + c * d.y;
		}

		const Vec2d t(half.x + (sprite.mirrorX ? -m.x : m.x), half.y + m.y);

		const int tx = static_cast<int>(floor(t.x));
		const int ty = static_cast<int>(floor(t.y));
		if(tx < 0 || ty < 0 || tx >= sprite.size.x || ty >= sprite.size.y) return false;

		*p_texelOut = Vec2i(tx, ty);
		return true;
	}
}

Sprites::Sprites()
{
	numSprites = 0;
	p_texture = 0;
}

void Sprites::clear()
{
	numSprites = 0;
}

Sprite& Sprites::add(const Vec2i& positionOnTexture)
{
	if(numSprites < MAX_SPRITES) numSprites++;

	Sprite& sprite = sprites[numSprites - 1];
	sprite = Sprite();
	sprite.positionOnTexture = positionOnTexture;
	return sprite;
}

Sprite& Sprites::add(const Vec2i& positionOnTexture,
					 const Vec4d& color)
{
	Sprite& sprite = add(positionOnTexture);
	sprite.color = color;
	return sprite;
}

int Sprites::getCount() const
{
	return numSprites;
}

const Sprite& Sprites::operator [] (int index) const
{
	return sprites[index];
}

void Sprites::setTexture(Texture* p_texture)
{
	this->p_texture = p_texture;
}

Texture* Sprites::getTexture() const
{
	return p_texture;
}

void Sprites::getFootprint(Vec2i* p_minOut,
						   Vec2i* p_maxOut) const
{
	if(!numSprites)
	{
		*p_minOut = Vec2i(0, 0);
		*p_maxOut = Vec2i(0, 0);
		return;
	}

	Vec2d lo(1.0e9, 1.0e9);
	Vec2d hi(-1.0e9, -1.0e9);

	for(int i = 0; i < numSprites; i++)
	{
		const Sprite& sprite = sprites[i];
		const Vec2d half(0.5 * sprite.size.x, 0.5 * sprite.size.y);
		const Vec2d centre(sprite.offset.x + half.x, sprite.offset.y + half.y);

		double c = 1.0;
		double s = 0.0;
		if(sprite.rotation != 0.0)
		{
			const double a = sprite.rotation * p_degToRad;
			c = cos(a);
			s = sin(a);
		}

		// Die vier Ecken drehen. Die Spiegelung bildet die Eckenmenge auf sich
		// selbst ab und aendert an der Huelle nichts.
		for(int k = 0; k < 4; k++)
		{
			const double x = (k & 1) ? half.x : -half.x;
			const double y = (k & 2) ? half.y : -half.y;
			const Vec2d p(centre.x + c * x - s * y, centre.y + s * x + c * y);
			lo.x = min(lo.x, p.x);
			lo.y = min(lo.y, p.y);
			hi.x = max(hi.x, p.x);
			hi.y = max(hi.y, p.y);
		}
	}

	*p_minOut = Vec2i(static_cast<int>(floor(lo.x + p_footprintEpsilon)),
					  static_cast<int>(floor(lo.y + p_footprintEpsilon)));
	*p_maxOut = Vec2i(static_cast<int>(ceil(hi.x - p_footprintEpsilon)),
					  static_cast<int>(ceil(hi.y - p_footprintEpsilon)));
}

int Sprites::getTryCount(int numParticles) const
{
	Vec2i lo, hi;
	getFootprint(&lo, &hi);

	const int area = (hi.x - lo.x) * (hi.y - lo.y);
	const int reference = Sprite::SIZE * Sprite::SIZE;
	if(area <= reference) return numParticles * DEBRIS_TRIES_PER_PARTICLE;

	return static_cast<int>((static_cast<double>(numParticles * DEBRIS_TRIES_PER_PARTICLE) * area) / reference);
}

bool Sprites::sample(Vec4d* p_colorOut,
					 Vec2i* p_offsetOut) const
{
	// Ohne Teilbilder oder ohne Pixel im Speicher entstehen keine Truemmer:
	// eine Kachel ohne Bild hat nichts zu zerstreuen.
	if(!numSprites || !p_texture || !p_texture->hasPixels()) return false;

	Vec2i lo, hi;
	getFootprint(&lo, &hi);
	const int w = hi.x - lo.x;
	const int h = hi.y - lo.y;
	if(w <= 0 || h <= 0) return false;

	// Eine einzige Zufallszahl reicht. randomInt() ist mt.randInt(0x7FFFFFFF),
	// und MTRand::randInt(n) maskiert bei einer Maske aus lauter Einsen nur -
	// es sind also 31 unverbrauchte Mersenne-Twister-Bits. Acht davon fuer die
	// Schwelle, die uebrigen 23 fuer die Stelle.
	const uint r = static_cast<uint>(randomInt());
	const uint threshold = r & 255;
	const uint where = r >> 8;
	const Vec2i point(lo.x + static_cast<int>(where % static_cast<uint>(w)),
					  lo.y + static_cast<int>((where / static_cast<uint>(w)) % static_cast<uint>(h)));

	// Von vorne nach hinten durch die Teilbilder, genau wie das Alphablending
	// beim Zeichnen: was vorne liegt, deckt zu einem Anteil seiner Deckkraft
	// ab. covered ist dieser Anteil, aufaddiert - eine einzige Schwelle daran
	// zu messen ist dasselbe wie jedes Teilbild einzeln auszuwuerfeln.
	double covered = 0.0;
	for(int i = numSprites - 1; i >= 0; i--)
	{
		const Sprite& sprite = sprites[i];

		Vec2i texel;
		if(!mapToTexel(sprite, point, &texel)) continue;

		const Vec4d pixel = p_texture->getPixel(sprite.positionOnTexture + texel);
		covered += (1.0 - covered) * pixel.a * sprite.color.a;

		// Annehmen mit der Wahrscheinlichkeit der Deckkraft. Ein voll deckendes
		// Pixel faellt damit in einem von 256 Faellen durch; das ist nicht zu
		// sehen und spart den Sonderfall.
		if(threshold < static_cast<uint>(covered * 255.0))
		{
			*p_colorOut = Vec4d(pixel.r * sprite.color.r,
								pixel.g * sprite.color.g,
								pixel.b * sprite.color.b,
								DEBRIS_ALPHA);
			*p_offsetOut = point;
			return true;
		}
	}

	return false;
}
