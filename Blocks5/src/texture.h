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

	// Give the pixels back: anything that reads them once at load time asks
	// for them, reads, and hands them back, and only a reader that keeps
	// asking - the debris sampling - holds on to them. Whoever did not ask
	// must not call this, or it takes the pixels from the one who did.
	void releasePixels();

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
	double matrix[16];
	Texture* p_parent;
};

#endif