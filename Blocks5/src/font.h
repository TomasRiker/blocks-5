#ifndef _FONT_H
#define _FONT_H

/*** Class for a font ***/

#include "resource.h"
// For QuadVertex, which a laid-out string is made of.
#include "renderer.h"

class Texture;

// A space of half a space's width, and a space in every other respect. It
// stands between the keycaps that belong together - the two keys of one
// action, and the two keys of a chord - where a full space either side of the
// slash or the plus pushes them apart, since each keycap already carries the
// padding of its own frame.
//
// A byte rather than an element like <k>, because a break is a matter of
// characters: adjustText() looks backwards for the last one it may cut at, and
// an element would have to be taught to be a break as well as to be skipped.
// The middle dot, because that is the character an editor shows a space as,
// and because it can be typed into languages.txt where the chords are written
// out - the same idiom as the pilcrow that means a line break there.
const unsigned char HALF_SPACE = '\xB7';

class Font : public Resource<Font>
{
	friend class Manager<Font>;

public:
	struct Options
	{
		int tabSize;
		int charSpacing;
		double lineSpacing;
		double charScaling;
		int shadows;
		int italic;
	};

	void reload();
	void cleanUp();

	// cache=false for a string whose layout will not be asked for again.
	void renderText(const std::string& text, const Vec2i& position, const Vec4d& color,
					bool cache = true);
	// p_outCharPositions gets one position per byte of the text and one
	// behind it, always text.length() + 1 of them.
	void measureText(const std::string& text, Vec2i* p_outDimensions, std::vector<Vec2i>* p_outCharPositions, const Vec2i& offset = Vec2i(0, 0));

	// renderText() neither clips nor wraps of its own accord, so text written
	// to a fixed place goes through one of these first: fitText() ends it in
	// three dots, adjustText() breaks it into lines.
	std::string fitText(const std::string& text, int maxWidth);
	std::string adjustText(const std::string& text, int maxWidth);

	int getLineHeight() const;

	const Options& getOptions() const;
	void setOptions(const Options& options);
	void pushOptions();
	void popOptions();

	Texture* getTexture();

	// What the two caches have done since the last reset. Counted always: is
	// the cache earning its memory cannot be asked of a build without them.
	struct CacheStats
	{
		uint hits;
		uint misses;
		uint evictions;
		// Calls to measureText(), and how many of them each cache answered
		// without walking the string. The rest are the walks.
		uint measures;
		uint measureHits;
		uint dimHits;
		uint dimEvictions;
		size_t entries;
		size_t quads;
		size_t dimEntries;
		size_t dimBytes;
	};

	static const CacheStats& getCacheStats() { return cacheStats; }
	static void resetCacheStats();

private:
	struct CharacterInfo
	{
		Vec2i position;
		Vec2i size;
	};

	// A laid-out string, ready to draw. The keycap frames are a batch of their
	// own because they carry no texture at all - four thin quads to a frame.
	struct StringCacheEntry
	{
		// A counter and not SDL_GetTicks(), which wraps at 49.7 days and
		// after that makes a cache evict what it has just built, for ever.
		uint lastUsed;
		// What measureText() answers for this string, measured once when the
		// entry is built. Set only on an entry that is kept.
		Vec2i dimensions;
		std::vector<QuadVertex> glyphs;
		std::vector<Vec2f> keyBoxes;
	};

	// What measureText() answered for a string nothing drew: the runs
	// adjustText() wraps and the candidates fitText() probes. No geometry,
	// which for those would be 64 bytes a character nothing ever draws.
	struct DimCacheEntry
	{
		uint lastUsed;
		Vec2i dimensions;
	};

	Font(const std::string& filename);
	~Font();

	static bool forceReload() { return false; }

	int getCharacterWidth(unsigned char c) const;

	// The rows a keycap frame occupies inside a glyph cell, as (top, height).
	Vec2i getKeyBoxRows() const;

	const StringCacheEntry& lookUpText(const std::string& text, bool cache);

	// Evict until the budget holds that much more. False where one string
	// wants more than the whole of it and is therefore not cached at all.
	static bool makeRoom(size_t quads);
	static bool makeDimRoom(size_t bytes);
	static size_t dimEntryBytes(const std::string& key);

	void rememberDimensions(const std::string& text, const Vec2i& dimensions);
	void forgetDimensions(const std::string& key);
	void buildText(const std::string& text, std::vector<QuadVertex>& glyphs, std::vector<Vec2f>& keyBoxes);
	void drawText(const StringCacheEntry& entry, const Vec4f& color) const;

	// Both caches key on this, and it is a reference into cacheKeyBuffer.
	const std::string& cacheKey(const std::string& text);

	int lineHeight;
	int offset;

	// The rows of a glyph cell a keycap frame runs from and to. lineHeight and
	// offset do not say this: a font may hang its line lower than its ink, and
	// a small font's letters may not fit inside its line at all.
	int capTop;
	int capBottom;

	CharacterInfo charInfo[256];
	Texture* p_texture;
	Options options;
	std::unordered_map<std::string, StringCacheEntry> stringCache;
	std::unordered_map<std::string, DimCacheEntry> dimCache;
	// Reused rather than returned by value: cacheKey() runs for every string
	// drawn in every frame, and a fresh string would be an allocation each.
	std::string cacheKeyBuffer;
	std::stack<Options> optionsStack;

	// Shared by every font, because what an entry costs is quads and not
	// slots: a keycap is twenty and a wrapped help page is thousands.
	static CacheStats cacheStats;

	// Every font alive, so eviction can take the oldest entry wherever it
	// lives - four fonts with a budget each is not one budget.
	static std::vector<Font*> liveFonts;

	// Ticks once per lookup.
	static uint lruClock;

	// Where a string goes that the caller said not to keep. One per font is
	// enough: renderText() draws from it three times, uninterrupted.
	StringCacheEntry scratchEntry;

	static size_t entryQuads(const StringCacheEntry& entry);
	void dropCache();
};

#endif