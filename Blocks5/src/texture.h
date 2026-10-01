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
	// because it decides how the texture is built before the upload.
	enum WrapMode
	{
		// Nothing samples this outside its own edges. Inside a page its
		// gutter copies its own edge, which is what GL_CLAMP_TO_EDGE returned.
		WM_CLAMP = 0,

		// Tiled by Renderer::tiledQuad, which cuts the quad at the picture's
		// edges, so it packs like a clamped one. Its gutter copies the
		// *opposite* edge, which is what GL_REPEAT returned there.
		WM_WRAP,

		// Tiled by GL_REPEAT, which wraps at the texture's edge, so this gets
		// a GL texture of its own. The weather: the rain's and the clouds' uv
		// is rotated, so the cuts tiledQuad would need are not axis-aligned;
		// the snow, only scaled and scrolled, is declared the same way.
		WM_REPEAT
	};

	// A request's options: the wrap mode in the low byte and flags above it,
	// as in Texture::WM_CLAMP | Texture::NEVER_PACK. NEVER_PACK keeps a
	// picture out of the atlas: one drawn on the loading screen and never
	// again, like logo.png, would hold its room in a page for the rest of the
	// session, or leave a hole in it when given back.
	static const int WRAP_MASK = 0xff;
	static const int NEVER_PACK = 0x100;

	void reload();
	void cleanUp();

	// The picture as a render state names it: the GL name, the texel scale
	// and origin that turn its own texels into uv, and its extent.
	TextureRef ref() const;
	const Vec2i& getSize() const;
	WrapMode getWrapMode() const;

	// A second request for a loaded picture, with that request's options.
	// Only the stricter direction reloads - to WM_REPEAT or to NEVER_PACK -
	// since the texture was built the other way; the reverse draws right as is.
	void reuseWithOptions(int options);

	// The atlas has moved this picture. Only the page and the origin change:
	// uv is written in the picture's own texels everywhere, so no cache built
	// from it has to be told.
	void movedTo(uint pageID, const Vec2i& origin);

	// A GL texture that is not a picture from a file - the atlas pages, the
	// framebuffer and the frame copies, the offscreen targets, the rewind's
	// noise - from pixels or empty where p_pixels is 0. It is here so that the
	// upload stays in this file; Renderer::deleteTexture takes it back.
	static uint createGLTexture(const Vec2i& size, const uchar* p_pixels, bool withAlpha, bool smooth, bool clamp);

	// A picture from memory, for the renderer's built-in block and disc. It
	// goes through place() like any other, so it packs and moves with a
	// repack. Not in the Manager, having no filename; the caller owns it.
	static Texture* createFromPixels(const Vec2i& size, const uchar* p_rgba, const std::string& name);

	// Frees the decoded pixels of every texture not asked to keep them, once
	// a logic tick from Engine::update(). Not at the end of reload(): both
	// keepInMemory() callers ask right after their request(), which would then
	// load twice. A sub-texture frees its own, since no sweep reaches it.
	static void freeUnkeptPixels();

	Texture* createSubTexture(const Vec2i& offset, const Vec2i& size, WrapMode wrapMode);
	void loadSubTexture(Texture* p_parent, const Vec2i& offset, const Vec2i& size);

	// Keeps the pixels for getPixel(), reloading if the sweep has already
	// freed them, so that no caller has to ask before the first sweep.
	void keepInMemory();
	Vec4f getPixel(const Vec2i& where) const;

	// Whether the pixels are still in memory. Without them getPixel() returns
	// transparent black and no error, so anything that reads pixels asks first.
	bool hasPixels() const;

private:
	Texture(const std::string& filename, int options);
	Texture(const Vec2i& size, const uchar* p_rgba, const std::string& name);
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
	// One texel in uv: 1/w and 1/h of the GL texture the picture lives in,
	// so 1/pageEdge when it is packed.
	Vec2f texelScale;
	// Where this picture begins inside texID, in uv; (0, 0) for a texture of
	// its own. A caller's texels become uv as texels * texelScale + uvOrigin.
	Vec2f uvOrigin;
	WrapMode wrapMode;
	bool neverPack;
	// False while the picture lives in an atlas page, which is not this
	// object's to delete: cleanUp() gives the rectangle back instead.
	bool ownsTexture;
	Texture* p_parent;
};

#endif