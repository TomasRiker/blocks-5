#ifndef _TEXTUREATLAS_H
#define _TEXTUREATLAS_H

/*** One GL texture holding many pictures, so that they share a draw ***/

#include "singleton.h"

class Texture;

// Nearly every draw call this game makes is a texture change (measured: 18 of
// a level frame's 24), so the fewer GL textures there are, the fewer draws.
// A picture that never samples outside its own edges can live inside a bigger
// one without noticing: uv is written in the picture's own texels either way,
// and Renderer::pushQuad turns those into the page's coordinates with the
// texel scale and the origin the picture's TextureRef carries.
//
// That is exactly why the wrap mode is declared at the request
// (Texture::WM_REPEAT): GL_REPEAT wraps at the *texture's* edge, so a picture
// in a page would read whatever was packed next door. Those keep a texture of
// their own. Everything else shares a page - including a picture the renderer
// tiles itself, because Renderer::tiledQuad cuts the quad so that no piece
// reads past an edge.
//
// Sampling is bit for bit what it was, and the reason is that a page's edge is
// a power of two: px/pageEdge and origin/pageEdge are both exact in float and
// their sum is exactly (px + origin)/pageEdge, so a quad lands on the same
// texel it landed on before. The frame oracle is what holds that claim.
class TextureAtlas : public Singleton<TextureAtlas>
{
	friend class Singleton<TextureAtlas>;

public:
	// A texel of gutter all round every picture, carrying its opposite edge.
	// Linear filtering reaches one texel past the coordinate it was given and
	// no further - there are no mipmaps anywhere in this game - so that one
	// texel is exactly what GL_REPEAT would have returned, which is the only
	// thing this game ever sampled with: GL_TEXTURE_WRAP_S and _T are set
	// nowhere in its history. Not approximately: the same texel value.
	static const int GUTTER = 1;

	// Where a picture ended up. pageID 0 means it is not in a page at all and
	// has a GL texture of its own - a tiling one, or one too big to pack.
	struct Slot
	{
		Slot() : pageID(0), origin(0, 0) {}
		uint pageID;
		Vec2i origin;
	};

	// Reserve room for a picture of this size and say where. The caller then
	// uploads it there itself, because the upload is raw GL and belongs in the
	// one file that owns it. False means it will not fit anywhere, and the
	// caller makes a texture of its own.
	bool reserve(Texture* p_texture, const Vec2i& size, Slot* p_slot);

	// Give a picture's rectangle back. The rectangle is remembered as free and
	// the atlas as fragmented; nothing moves here, because a skin change
	// releases eleven pictures in a row and reloads them, and compacting after
	// each would be eleven repacks for the same answer.
	void giveBack(Texture* p_texture);

	// Compact, if the holes are worth the work. Called once a logic tick from
	// Engine::update(), which is a point where the renderer holds nothing.
	void repackIfWorthwhile();

	// Delete the pages. Native only, like every other teardown.
	void exit();

	// For the log and the test hook.
	int getPageCount() const;
	int getPageEdge() const;
	int getUsedArea() const;
	int getWastedArea() const;
	int getRepackCount() const;

private:
	TextureAtlas();
	~TextureAtlas();

	struct Rect
	{
		Rect() : origin(0, 0), size(0, 0) {}
		Rect(const Vec2i& origin, const Vec2i& size) : origin(origin), size(size) {}
		Vec2i origin;
		Vec2i size;
	};

	struct Page
	{
		uint id;
		std::vector<Rect> freeRects;
	};

	// A picture that is in a page, and where. The texture pointer is what a
	// repack writes the new place back through.
	struct Entry
	{
		Texture* p_texture;
		int page;       // an index into pages, not a GL name
		Rect rect;      // including the gutter, which is what was reserved
	};

	bool makePage();
	// Give a rectangle back to a page's free list, joined to any neighbour it
	// makes a rectangle with. Without that the list only ever grows finer and
	// the atlas fragments itself out of room in a few screen changes.
	void addFreeRect(Page& page, const Rect& rect);
	// Guillotine: the free rectangle that wastes least, split in two. Nothing
	// is coalesced on the way out, which is what the repack is for.
	bool takeRect(Page& page, const Vec2i& size, Vec2i* p_origin);
	void repack();

	std::vector<Page> pages;
	std::vector<Entry> entries;
	int pageEdge;
	int repacks;
	bool compactionWanted;
};

#endif
