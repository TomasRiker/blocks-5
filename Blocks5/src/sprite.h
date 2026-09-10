#ifndef _SPRITE_H
#define _SPRITE_H

/*** Classes for the appearance of an object or a tile ***/

class Texture;

// Opacity a debris particle starts with.
const double DEBRIS_ALPHA = 0.25;

// Tries per desired particle; the dial for the density of the cloud.
const int DEBRIS_TRIES_PER_PARTICLE = 2;

// One sprite. color is the object's own tint, not the render pass's -
// renderSprites() multiplies that one in.
struct Sprite
{
	static const int SIZE = 16;

	Vec2i positionOnTexture;
	Vec2i size;
	Vec2i offset;
	Vec4d color;
	bool mirrorX;
	double rotation;

	Sprite();
};

class Sprites
{
public:
	static const int MAX_SPRITES = 4;

	Sprites();

	void clear();
	Sprite& add(const Vec2i& positionOnTexture);
	Sprite& add(const Vec2i& positionOnTexture, const Vec4d& color);

	int getCount() const;
	const Sprite& operator [] (int index) const;

	void setTexture(Texture* p_texture);
	Texture* getTexture() const;

	// Smallest axis-aligned rectangle in object coordinates holding every
	// sprite with its rotation and offset.
	void getFootprint(Vec2i* p_minOut, Vec2i* p_maxOut) const;

	// Tries for numParticles particles on average; grows with the area.
	int getTryCount(int numParticles) const;

	// Rejection sampling for the colour of a debris particle. false means:
	// there is nothing at the spot the try landed on.
	bool sample(Vec4d* p_colorOut, Vec2i* p_offsetOut) const;

private:
	Sprite sprites[MAX_SPRITES];
	int numSprites;
	Texture* p_texture;
};

#endif
