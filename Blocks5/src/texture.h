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

	// Texturing on, this picture bound, and the texture matrix set to its own
	// 1/w, 1/h - all three through GL::, which issues only what has moved.
	// There is no unbind(): switching texturing off says nothing about this
	// texture or any other, so it is GL::setTexturing(false) at the caller.
	void bind() const;
	const Vec2i& getSize() const;

	// Hand the decoded pixels back for every texture that was not asked to
	// keep them. Once a logic tick, from Engine::update().
	//
	// Not at the end of reload(): both keepInMemory() callers ask immediately
	// after their request(), and each would then force a second load. And not
	// inside bind(), which is called thousands of times a frame and is about
	// drawing. A sub-texture frees at the end of its own load instead: it is
	// not in the Manager, so no sweep reaches it, and nothing ever asks to
	// keep one.
	static void freeUnkeptPixels();

	Texture* createSubTexture(const Vec2i& offset, const Vec2i& size);
	void loadSubTexture(Texture* p_parent, const Vec2i& offset, const Vec2i& size);

	// Keep the pixels in memory for getPixel() to read. Reloads the texture if
	// the sweep has already freed them - the flag alone brings nothing back.
	// Without that, the debris sampling would depend on every caller making
	// this promise before the first sweep.
	void keepInMemory();
	Vec4d getPixel(const Vec2i& where) const;

	// Are the pixels still in memory? freeUnkeptPixels() hands back everything
	// keepInMemory() was not called on, and getPixel() then returns transparent
	// black for everything - no error, simply wrong. Anything that reads pixels
	// asks first.
	bool hasPixels() const;

private:
	Texture(const std::string& filename);
	~Texture();

	// Sets GL_TEXTURE_WRAP_S/T if the edge lengths are not powers of two. Must
	// run while the texture is bound.
	void applyWrapMode() const;
	void checkDimensions();
	void freePixels();

	static bool forceReload() { return false; }

	SDL_Surface* p_rgba;
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