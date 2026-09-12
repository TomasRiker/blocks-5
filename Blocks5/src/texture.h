#ifndef _TEXTURE_H
#define _TEXTURE_H

/*** Class for a texture ***/

#include "resource.h"

class Texture : public Resource<Texture>
{
	friend class Manager<Texture>;

public:
	void reload();
	void cleanUp();

	void bind() const;
	void unbind() const;
	const Vec2i& getSize() const;

	Texture* createSubTexture(const Vec2i& offset, const Vec2i& size);
	void loadSubTexture(Texture* p_parent, const Vec2i& offset, const Vec2i& size);

	// Keep the pixels in memory for getPixel() to read. Reloads the texture if
	// bind() has already freed them - the flag alone brings nothing back.
	// Without that, the debris sampling would depend on every caller making
	// this promise before the first bind().
	void keepInMemory();
	Vec4d getPixel(const Vec2i& where) const;

	// Are the pixels still in memory? bind() frees them unless keepInMemory()
	// has been called, and getPixel() then returns transparent black for
	// everything - no error, simply wrong. Anything that reads pixels asks
	// first.
	bool hasPixels() const;

private:
	Texture(const std::string& filename);
	~Texture();

	// Sets GL_TEXTURE_WRAP_S/T if the edge lengths are not powers of two. Must
	// run while the texture is bound.
	void applyWrapMode() const;
	void checkDimensions();

	static bool forceReload() { return false; }

	mutable SDL_Surface* p_rgba;
	unsigned int texID;
	Vec2i offset;
	Vec2i size;
	bool doKeepInMemory;
	// What one texel of this picture is worth, 1/w and 1/h. Every texture
	// matrix this game ever samples a sprite under is that diagonal and
	// nothing else, so it is kept as the two numbers it is made of rather
	// than as sixteen.
	Vec2d texelScale;
	Texture* p_parent;
};

#endif