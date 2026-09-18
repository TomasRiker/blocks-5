#include "pch.h"
#include "sprite.h"
#include "texture.h"
#include "tileset.h"

// The cell sampled from is the same for tiles as for object sprites. Were the
// two ever to differ, getTryCount() would have to take a different reference
// area.
static_assert(Sprite::SIZE == TileSet::TILE_SIZE,
			  "debris sampling assumes tiles and object sprites share a cell size");

Sprite::Sprite()
{
	positionOnTexture = Vec2i(0, 0);
	size = Vec2i(SIZE, SIZE);
	offset = Vec2i(0, 0);
	color = Vec4f(1.0f, 1.0f, 1.0f, 1.0f);
	mirrorX = false;
	rotation = 0.0f;
}

namespace
{
	const float degToRad = 3.1415926535897932384626433832795f / 180.0f;

	// What the footprint is trimmed by before floor() and ceil() go at it.
	// cos(90 degrees) is -4.4e-8 and not 0, the left edge of a quarter turn
	// comes out as -4.8e-7, and the cell would be 17x17 for no reason -
	// getTryCount() works from the area and would roll 13% more debris than the
	// same sprite unrotated. The error grows with the sprite's half-width, and
	// the one sprite larger than a cell - the burnt ground - is turned by an
	// arbitrary angle, where no edge lands on a whole number to begin with.
	const float footprintEpsilon = 1.0e-6f;

	// Trace the spot in object coordinates back into the cell renderSprite
	// fetched it from: first the offset, then the inverse rotation, then the
	// mirroring, which is its own inverse. Because it is the same matrix in the
	// same frame of reference, the result holds whichever way the y axis
	// points. Computed with pixel centres and floor() - otherwise every
	// rotation would lose half a row of pixels.
	bool mapToTexel(const Sprite& sprite,
					const Vec2i& point,
					Vec2i* p_texelOut)
	{
		const Vec2f half(0.5f * sprite.size.x, 0.5f * sprite.size.y);
		const Vec2f d(point.x + 0.5f - sprite.offset.x - half.x,
					  point.y + 0.5f - sprite.offset.y - half.y);

		Vec2f m(d);
		if(sprite.rotation != 0.0f)
		{
			const float a = sprite.rotation * degToRad;
			const float c = cosf(a);
			const float s = sinf(a);
			m.x =  c * d.x + s * d.y;
			m.y = -s * d.x + c * d.y;
		}

		const Vec2f t(half.x + (sprite.mirrorX ? -m.x : m.x), half.y + m.y);

		const int tx = static_cast<int>(floorf(t.x));
		const int ty = static_cast<int>(floorf(t.y));
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
	// Past MAX_SPRITES the last slot is reused rather than the array overrun.
	if(numSprites < MAX_SPRITES) numSprites++;

	Sprite& sprite = sprites[numSprites - 1];
	sprite = Sprite();
	sprite.positionOnTexture = positionOnTexture;
	return sprite;
}

Sprite& Sprites::add(const Vec2i& positionOnTexture,
					 const Vec4f& color)
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

	Vec2f lo(1.0e9f, 1.0e9f);
	Vec2f hi(-1.0e9f, -1.0e9f);

	for(int i = 0; i < numSprites; i++)
	{
		const Sprite& sprite = sprites[i];
		const Vec2f half(0.5f * sprite.size.x, 0.5f * sprite.size.y);
		const Vec2f centre(sprite.offset.x + half.x, sprite.offset.y + half.y);

		float c = 1.0f;
		float s = 0.0f;
		if(sprite.rotation != 0.0f)
		{
			const float a = sprite.rotation * degToRad;
			c = cosf(a);
			s = sinf(a);
		}

		// Rotate the four corners. The mirroring maps the set of corners onto
		// itself and changes nothing about the footprint.
		for(int k = 0; k < 4; k++)
		{
			const float x = (k & 1) ? half.x : -half.x;
			const float y = (k & 2) ? half.y : -half.y;
			const Vec2f p(centre.x + c * x - s * y, centre.y + s * x + c * y);
			lo.x = min(lo.x, p.x);
			lo.y = min(lo.y, p.y);
			hi.x = max(hi.x, p.x);
			hi.y = max(hi.y, p.y);
		}
	}

	*p_minOut = Vec2i(static_cast<int>(floorf(lo.x + footprintEpsilon)),
					  static_cast<int>(floorf(lo.y + footprintEpsilon)));
	*p_maxOut = Vec2i(static_cast<int>(ceilf(hi.x - footprintEpsilon)),
					  static_cast<int>(ceilf(hi.y - footprintEpsilon)));
}

int Sprites::getTryCount(int numParticles) const
{
	Vec2i lo, hi;
	getFootprint(&lo, &hi);

	const int area = (hi.x - lo.x) * (hi.y - lo.y);
	const int reference = Sprite::SIZE * Sprite::SIZE;
	if(area <= reference) return numParticles * DEBRIS_TRIES_PER_PARTICLE;

	return static_cast<int>((static_cast<float>(numParticles * DEBRIS_TRIES_PER_PARTICLE) * area) / reference);
}

bool Sprites::sample(Vec4f* p_colorOut,
					 Vec2i* p_offsetOut) const
{
	// Without sprites, or without pixels in memory, there is no debris: a tile
	// with no image has nothing to scatter.
	if(!numSprites || !p_texture || !p_texture->hasPixels()) return false;

	Vec2i lo, hi;
	getFootprint(&lo, &hi);
	const int w = hi.x - lo.x;
	const int h = hi.y - lo.y;
	if(w <= 0 || h <= 0) return false;

	// A single random number is enough. randomInt() is mt.randInt(0x7FFFFFFF),
	// and with a mask of all ones MTRand::randInt(n) only masks - leaving 31
	// unspent Mersenne Twister bits. Eight of them for the threshold, the
	// remaining 23 for the spot.
	const uint r = static_cast<uint>(randomInt());
	const uint threshold = r & 255;
	const uint where = r >> 8;
	const Vec2i point(lo.x + static_cast<int>(where % static_cast<uint>(w)),
					  lo.y + static_cast<int>((where / static_cast<uint>(w)) % static_cast<uint>(h)));

	// Front to back through the sprites, exactly like the alpha blending when
	// drawing: what lies in front covers by a share of its opacity. covered is
	// that share, accumulated - measuring one threshold against it is the same
	// as rolling for each sprite separately.
	float covered = 0.0f;
	for(int i = numSprites - 1; i >= 0; i--)
	{
		const Sprite& sprite = sprites[i];

		Vec2i texel;
		if(!mapToTexel(sprite, point, &texel)) continue;

		const Vec4f pixel = p_texture->getPixel(sprite.positionOnTexture + texel);
		covered += (1.0f - covered) * pixel.a * sprite.color.a;

		// Accept with the probability of the opacity. A fully opaque pixel
		// therefore fails in one of 256 cases; that cannot be seen and saves
		// the special case.
		if(threshold < static_cast<uint>(covered * 255.0f))
		{
			*p_colorOut = Vec4f(pixel.r * sprite.color.r,
								pixel.g * sprite.color.g,
								pixel.b * sprite.color.b,
								DEBRIS_ALPHA);
			*p_offsetOut = point;
			return true;
		}
	}

	return false;
}
