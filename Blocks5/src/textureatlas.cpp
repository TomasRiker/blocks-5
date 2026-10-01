#include "pch.h"
#include "textureatlas.h"
#include "texture.h"
#include "glextensions.h"
#include "engine.h"

namespace
{
	// A page is 2048 square where the machine allows it, 16 MB of RGBA, and
	// not the largest it would take: the resident set of pictures is about
	// 6.2 Mtexel, which two of these hold with a fifth to spare, where one
	// 4096 page would allocate 64 MB to keep 25 MB. At most four pages, so a
	// runaway cannot eat the graphics memory; a picture that finds no room
	// keeps a texture of its own.
	const int MAX_PAGE_EDGE = 2048;
	const int MAX_PAGES = 4;
}

TextureAtlas::TextureAtlas()
{
	pageEdge = 0;
	repacks = 0;
	compactionWanted = false;
}

TextureAtlas::~TextureAtlas()
{
}

int TextureAtlas::getPageCount() const { return static_cast<int>(pages.size()); }
int TextureAtlas::getPageEdge() const { return pageEdge; }

uint TextureAtlas::getPageID(int index) const
{
	if(index < 0 || index >= static_cast<int>(pages.size())) return 0;
	return pages[index].id;
}
int TextureAtlas::getRepackCount() const { return repacks; }

int TextureAtlas::getUsedArea() const
{
	int area = 0;
	for(uint i = 0; i < entries.size(); i++) area += entries[i].rect.size.x * entries[i].rect.size.y;
	return area;
}

int TextureAtlas::getWastedArea() const
{
	return static_cast<int>(pages.size()) * pageEdge * pageEdge - getUsedArea();
}

void TextureAtlas::addFreeRect(Page& page, const Rect& rect)
{
	// Two free rectangles make a third only when they share a whole edge:
	// side by side at the same height, or stacked at the same width. Joining
	// them is what lets a picture that was given back be asked for again -
	// which is the whole of what a skin change does - without the room it
	// left behind having been cut into pieces too small to hold it.
	Rect r = rect;
	for(bool joined = true; joined; )
	{
		joined = false;
		for(uint i = 0; i < page.freeRects.size(); i++)
		{
			const Rect& o = page.freeRects[i];
			const bool sameRow = (o.origin.y == r.origin.y && o.size.y == r.size.y);
			const bool sameColumn = (o.origin.x == r.origin.x && o.size.x == r.size.x);
			if(sameRow && o.origin.x + o.size.x == r.origin.x)
			{
				r.origin.x = o.origin.x;
				r.size.x += o.size.x;
			}
			else if(sameRow && r.origin.x + r.size.x == o.origin.x) r.size.x += o.size.x;
			else if(sameColumn && o.origin.y + o.size.y == r.origin.y)
			{
				r.origin.y = o.origin.y;
				r.size.y += o.size.y;
			}
			else if(sameColumn && r.origin.y + r.size.y == o.origin.y) r.size.y += o.size.y;
			else continue;

			page.freeRects.erase(page.freeRects.begin() + i);
			joined = true;
			break;
		}
	}
	page.freeRects.push_back(r);
}

bool TextureAtlas::makePage()
{
	if(static_cast<int>(pages.size()) >= MAX_PAGES) return false;

	if(!pageEdge)
	{
		// Asked once, and only here: GLExtensions::init() has long since read
		// what the machine takes, and the first page is the first time it
		// matters.
		pageEdge = min(MAX_PAGE_EDGE, GLExtensions::maxTextureSize());
		if(pageEdge < 1024)
		{
			printfLog("- WARNING: The largest texture this machine takes is %d, so pictures are not packed.\n",
					  GLExtensions::maxTextureSize());
			pageEdge = 0;
			return false;
		}
	}

	// Empty, clamped and linear, like every picture it will hold. Nothing ever
	// samples a page outside itself: the gutter is what the edges are for.
	const uint id = Texture::createGLTexture(Vec2i(pageEdge, pageEdge), 0, true, true, true);
	if(!id) return false;

	Page page;
	page.id = id;
	page.freeRects.push_back(Rect(Vec2i(0, 0), Vec2i(pageEdge, pageEdge)));
	pages.push_back(page);
	printfLog("> INFO: Texture atlas: page %d is %dx%d (GL name %u).\n",
			  static_cast<int>(pages.size()), pageEdge, pageEdge, id);
	return true;
}

bool TextureAtlas::takeRect(Page& page,
							const Vec2i& size,
							Vec2i* p_origin)
{
	// The free rectangle that is left over least, which keeps the big ones
	// whole for the big pictures.
	int best = -1;
	int bestWaste = 0;
	for(uint i = 0; i < page.freeRects.size(); i++)
	{
		const Rect& r = page.freeRects[i];
		if(r.size.x < size.x || r.size.y < size.y) continue;
		const int waste = r.size.x * r.size.y - size.x * size.y;
		if(best < 0 || waste < bestWaste)
		{
			best = static_cast<int>(i);
			bestWaste = waste;
		}
	}
	if(best < 0) return false;

	const Rect r = page.freeRects[best];
	page.freeRects.erase(page.freeRects.begin() + best);
	*p_origin = r.origin;

	// Guillotine: one cut across the whole free rectangle and one along what
	// is left of the other side, taking whichever direction leaves the wider
	// of the two pieces in one piece.
	Rect a, b;
	if(r.size.x - size.x >= r.size.y - size.y)
	{
		a = Rect(Vec2i(r.origin.x + size.x, r.origin.y), Vec2i(r.size.x - size.x, r.size.y));
		b = Rect(Vec2i(r.origin.x, r.origin.y + size.y), Vec2i(size.x, r.size.y - size.y));
	}
	else
	{
		a = Rect(Vec2i(r.origin.x + size.x, r.origin.y), Vec2i(r.size.x - size.x, size.y));
		b = Rect(Vec2i(r.origin.x, r.origin.y + size.y), Vec2i(r.size.x, r.size.y - size.y));
	}
	if(a.size.x > 0 && a.size.y > 0) addFreeRect(page, a);
	if(b.size.x > 0 && b.size.y > 0) addFreeRect(page, b);
	return true;
}

bool TextureAtlas::reserve(Texture* p_texture,
						   const Vec2i& size,
						   Slot* p_slot)
{
	const Vec2i needed = size + Vec2i(2 * GUTTER, 2 * GUTTER);
	if(pageEdge && (needed.x > pageEdge || needed.y > pageEdge)) return false;

	for(int attempt = 0; attempt < 2; attempt++)
	{
		for(uint i = 0; i < pages.size(); i++)
		{
			Vec2i origin;
			if(!takeRect(pages[i], needed, &origin)) continue;

			Entry entry;
			entry.p_texture = p_texture;
			entry.page = static_cast<int>(i);
			entry.rect = Rect(origin, needed);
			entries.push_back(entry);

			p_slot->pageID = pages[i].id;
			p_slot->origin = origin + Vec2i(GUTTER, GUTTER);
			return true;
		}

		// No page had room. Where the room existed but in pieces, a repack
		// would find it - not here, in the middle of a load, but at the top of
		// the next tick, when the renderer holds nothing and a skin change's
		// eleven reloads are done.
		int freeArea = 0;
		for(uint i = 0; i < pages.size(); i++)
			for(uint j = 0; j < pages[i].freeRects.size(); j++)
				freeArea += pages[i].freeRects[j].size.x * pages[i].freeRects[j].size.y;
		if(freeArea >= needed.x * needed.y) compactionWanted = true;

		if(attempt || !makePage()) break;
	}

	return false;
}

void TextureAtlas::giveBack(Texture* p_texture)
{
	for(uint i = 0; i < entries.size(); i++)
	{
		if(entries[i].p_texture != p_texture) continue;

		addFreeRect(pages[entries[i].page], entries[i].rect);
		entries.erase(entries.begin() + i);

		// No compacting here: a hole costs nothing until something needs the
		// room, and a skin change gives eleven pictures back only to ask for
		// eleven of the same sizes.
		return;
	}
}

void TextureAtlas::repackIfWorthwhile()
{
	if(!compactionWanted) return;
	compactionWanted = false;
	repack();
}

void TextureAtlas::repack()
{
	if(entries.empty() || !pageEdge) return;

	// Biggest first, which is what makes a guillotine packer pack at all.
	// The order is by the longer edge rather than by area: what runs a page
	// out is a picture that is wide or tall, not one that is large.
	std::vector<Entry> sorted = entries;
	for(uint i = 1; i < sorted.size(); i++)
	{
		Entry key = sorted[i];
		const int keyEdge = max(key.rect.size.x, key.rect.size.y);
		uint j = i;
		while(j && max(sorted[j - 1].rect.size.x, sorted[j - 1].rect.size.y) < keyEdge)
		{
			sorted[j] = sorted[j - 1];
			j--;
		}
		sorted[j] = key;
	}

	// Pack into a fresh set of pages on paper first. Nothing is created and
	// nothing moves until the whole layout is known, so a packing that cannot
	// be done leaves the atlas exactly as it was.
	std::vector<Page> newPages;
	std::vector<Entry> newEntries;
	for(uint i = 0; i < sorted.size(); i++)
	{
		bool placed = false;
		for(int pass = 0; pass < 2 && !placed; pass++)
		{
			for(uint p = 0; p < newPages.size(); p++)
			{
				Vec2i origin;
				if(!takeRect(newPages[p], sorted[i].rect.size, &origin)) continue;
				Entry entry = sorted[i];
				entry.page = static_cast<int>(p);
				entry.rect.origin = origin;
				newEntries.push_back(entry);
				placed = true;
				break;
			}
			if(placed || pass) break;
			if(static_cast<int>(newPages.size()) >= MAX_PAGES) break;
			Page page;
			page.id = 0;
			page.freeRects.push_back(Rect(Vec2i(0, 0), Vec2i(pageEdge, pageEdge)));
			newPages.push_back(page);
		}
		if(!placed)
		{
			printfLog("- WARNING: Texture atlas: a repack of %d pictures did not fit; keeping the old layout.\n",
					  static_cast<int>(sorted.size()));
			return;
		}
	}

	// It fits. Make the new pages for real, and stop if the machine will not
	// give them - again leaving everything as it was.
	for(uint p = 0; p < newPages.size(); p++)
	{
		newPages[p].id = Texture::createGLTexture(Vec2i(pageEdge, pageEdge), 0, true, true, true);
		if(newPages[p].id) continue;

		printfLog("- WARNING: Texture atlas: no room for a page to repack into; keeping the old layout.\n");
		for(uint q = 0; q < p; q++) Renderer::inst().deleteTexture(newPages[q].id);
		return;
	}

	// Copy the texels page to page, each rectangle with its gutter. A copy and
	// not a render pass: glCopyTexSubImage2D works in GL's own coordinates at
	// both ends, where a render pass would go through a projection whose y
	// runs the other way.
	Engine& engine = Engine::inst();
	Renderer& renderer = Renderer::inst();
	for(uint p = 0; p < pages.size(); p++)
	{
		if(!engine.beginRenderToTexture(pages[p].id, Vec2i(pageEdge, pageEdge))) continue;
		for(uint i = 0; i < newEntries.size(); i++)
		{
			const Entry& to = newEntries[i];
			int from = -1;
			for(uint j = 0; j < entries.size(); j++)
				if(entries[j].p_texture == to.p_texture) { from = static_cast<int>(j); break; }
			if(from < 0 || entries[from].page != static_cast<int>(p)) continue;
			renderer.copyRegion(newPages[to.page].id, to.rect.origin, entries[from].rect.origin, to.rect.size);
		}
		engine.endRenderToTexture();
	}

	for(uint p = 0; p < pages.size(); p++) renderer.deleteTexture(pages[p].id);
	pages = newPages;
	entries = newEntries;

	// Tell every picture where it is now. Nothing else has to be told: a uv
	// is written in the picture's own texels and turned into the page's at
	// submission, so the tile grid's cache, the font's and the lightning's
	// are all still right.
	for(uint i = 0; i < entries.size(); i++)
		entries[i].p_texture->movedTo(pages[entries[i].page].id, entries[i].rect.origin + Vec2i(GUTTER, GUTTER));

	repacks++;
	printfLog("> INFO: Texture atlas: repacked %d pictures into %d page(s).\n",
			  static_cast<int>(entries.size()), static_cast<int>(pages.size()));
}

void TextureAtlas::exit()
{
	for(uint p = 0; p < pages.size(); p++) Renderer::inst().deleteTexture(pages[p].id);
	pages.clear();
	entries.clear();
}
