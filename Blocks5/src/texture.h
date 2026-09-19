#ifndef _TEXTURE_H
#define _TEXTURE_H

/*** Class for a texture ***/

#include "resource.h"
#include "renderstate.h"

class Texture : public Resource<Texture>
{
	friend class Manager<Texture>;

public:
	// How the picture is sampled outside its own edges, given at the request
	// rather than set afterwards: it decides how the texture is built, and
	// that has to be known before the upload. Two of the three are the GL
	// modes of the same name; the middle one is the renderer wrapping the
	// picture itself, which is what lets a tiling picture share a page.
	enum WrapMode
	{
		// Nothing samples this outside its own edges. Inside a page its
		// gutter copies its own edge, which is what GL_CLAMP_TO_EDGE returned.
		WM_CLAMP = 0,

		// Tiled, but by Renderer::tiledQuad, which cuts the quad at the
		// picture's edges so that every piece samples one copy of it. No
		// piece reads past an edge, so this packs like a clamped one, and its
		// gutter copies the *opposite* edge - which is what GL_REPEAT
		// returned for the one texel linear filtering reaches there.
		WM_WRAP,

		// Tiled by GL_REPEAT, which wraps at the texture's edge and not at
		// the picture's, so this needs a GL texture to itself and can never
		// share a page. The weather: scrolled without bound and rotated with
		// it, so the cuts a split would need are not axis-aligned.
		WM_REPEAT
	};

	// Everything a request may say about a picture: a wrap mode, and flags on
	// top of it. The wrap mode is the low byte, so a caller writes
	// Texture::WM_CLAMP | Texture::NEVER_PACK and both arrive.
	//
	// NEVER_PACK keeps a picture out of the atlas whatever its wrap mode. A
	// page is the wrong home for one that is drawn on the loading screen and
	// then not again: it would hold half a megatexel for the rest of the
	// session, and giving it back leaves a hole the atlas has to repack
	// around. The pages are for what a frame draws over and over.
	static const int WRAP_MASK = 0xff;
	static const int NEVER_PACK = 0x100;

	void reload();
	void cleanUp();

	// The picture as a render state names it: the GL name and 1/w, 1/h,
	// which is what makes texture coordinates read in its own texels.
	TextureRef ref() const;
	const Vec2i& getSize() const;
	WrapMode getWrapMode() const;

	// A second request for a picture already loaded, with the options that
	// request asked for. Only the stricter direction does anything - to
	// WM_REPEAT, or to NEVER_PACK - and it reloads for the same reason
	// keepInMemory() does: the flag alone does not move a texture that has
	// already been built the other way. The other direction needs nothing: a
	// tiling texture drawn without tiling is right, only not packed, and a
	// picture kept out of a page draws the same as one in it.
	void reuseWithOptions(int options);

	// The atlas has moved this picture to another page, or another place in
	// the same one. Only the texel scale and the origin change: uv is written
	// in the picture's own texels everywhere in the tree, so no cache built
	// from it has to be told.
	void movedTo(uint pageID, const Vec2i& origin);

	// A GL texture that is not a picture from a file: the frame copies and
	// the rewind's noise, from pixels or empty where p_pixels is 0, RGBA or
	// RGB, sampled linearly or not, clamped or repeating. The one place a
	// texture is made besides reload(), so that the upload stays in this
	// file; Renderer::deleteTexture takes it back.
	static uint createGLTexture(const Vec2i& size, const uchar* p_pixels, bool withAlpha, bool smooth, bool clamp);

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

	Texture* createSubTexture(const Vec2i& offset, const Vec2i& size, WrapMode wrapMode);
	void loadSubTexture(Texture* p_parent, const Vec2i& offset, const Vec2i& size);

	// Keep the pixels in memory for getPixel() to read. Reloads the texture if
	// the sweep has already freed them - the flag alone brings nothing back.
	// Without that, the debris sampling would depend on every caller making
	// this promise before the first sweep.
	void keepInMemory();
	Vec4f getPixel(const Vec2i& where) const;

	// Are the pixels still in memory? freeUnkeptPixels() hands back everything
	// keepInMemory() was not called on, and getPixel() then returns transparent
	// black for everything - no error, simply wrong. Anything that reads pixels
	// asks first.
	bool hasPixels() const;

private:
	Texture(const std::string& filename, int options);
	// A part of another picture, for createSubTexture(): copies the region
	// straight out of the parent's pixels, without decoding the file first.
	Texture(Texture* p_parent, const Vec2i& offset, const Vec2i& size, WrapMode wrapMode);
	~Texture();

	// Sets GL_TEXTURE_WRAP_S/T from the declared wrap mode. Must run while the
	// texture is bound.
	void applyWrapMode() const;
	// The pixels are ready; this decides where they go - a page shared with
	// other pictures, or a GL texture of this one's own - and uploads them.
	void place();
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
	Vec2f texelScale;
	// Where this picture begins inside texID, which is (0, 0) for a texture of
	// its own. Together with texelScale - 1/pageEdge when it is in a page,
	// 1/size when it is not - that is the whole of what a caller's texels have
	// to be put through.
	Vec2f uvOrigin;
	WrapMode wrapMode;
	bool neverPack;
	// False while the picture lives in an atlas page, which is not this
	// object's to delete: cleanUp() gives the rectangle back instead.
	bool ownsTexture;
	Texture* p_parent;
};

#endif