#ifndef _TEXTUREATLAS_H
#define _TEXTUREATLAS_H

/*** One GL texture holding many pictures, so that they share a draw ***/

#include "singleton.h"

class Texture;

// Without pages most draw calls are texture changes (measured: 18 of a level
// frame's 24), so fewer GL textures means fewer draws. A picture that never
// samples outside its own edges can live inside a bigger one unnoticed: uv is
// written in the picture's own texels, and Renderer::pushQuad turns those into
// the page's with the texel scale and origin its TextureRef carries.
//
// That is why the wrap mode is declared at the request: GL_REPEAT wraps at the
// *texture's* edge, so a WM_REPEAT picture in a page would read whatever was
// packed next door, and it keeps a texture of its own. A WM_WRAP picture does
// share a page, because Renderer::tiledQuad cuts the quad so that no piece
// reads past an edge; its gutter copies the opposite edge rather than its own.
//
// The page coordinate is exact because a page's edge is a power of two:
// px/pageEdge and origin/pageEdge are both exact in float, and their sum
// rounds exactly as (px + origin)/pageEdge does.
class TextureAtlas : public Singleton<TextureAtlas>
{
	friend class Singleton<TextureAtlas>;

public:
	// A texel of gutter all round every picture. Linear filtering reaches one
	// texel past its coordinate and no further, and there are no mipmaps, so
	// the picture's edge copied outwards returns exactly the texel
	// GL_CLAMP_TO_EDGE would.
	static const int GUTTER = 1;

	// Where a picture ended up. pageID 0 means it is in no page and has a GL
	// texture of its own: a WM_REPEAT or NEVER_PACK one, or one that found no
	// room.
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

	// Give a picture's rectangle back to its page's free list. Nothing moves
	// here: a skin change releases eleven pictures in a row and reloads them,
	// and compacting after each would be eleven repacks for the same answer.
	void giveBack(Texture* p_texture);

	// Compact if, since the last call, a picture found no room although the
	// free rectangles added up to enough: a repack cannot make room that is
	// not there. Called once a logic tick from Engine::update(), a point
	// where the renderer holds nothing.
	void repackIfWorthwhile();

	// Delete the pages. Native only, like every other teardown.
	void exit();

	// For the log and the test hook.
	int getPageCount() const;
	int getPageEdge() const;
	// The GL name of a page, so that -perf's Ctrl+Shift+F9 can read one back
	// and write it out. Nothing else needs it: a picture reaches its page
	// through its own TextureRef.
	uint getPageID(int index) const;
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
	// Guillotine: the free rectangle that wastes least, split in two, and the
	// two pieces handed back through addFreeRect().
	bool takeRect(Page& page, const Vec2i& size, Vec2i* p_origin);
	void repack();

	std::vector<Page> pages;
	std::vector<Entry> entries;
	int pageEdge;
	int repacks;
	bool compactionWanted;
};

#endif
