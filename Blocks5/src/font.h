#ifndef _FONT_H
#define _FONT_H

/*** Class for a font ***/

#include "resource.h"

class Texture;

// A space of half a space's width, and a space in every other respect: it is
// measured like one and a line breaks at one, replacing it exactly as a break
// replaces a space. It stands between the keycaps that belong together - the
// two keys of one action, and the two keys of a chord - where a full space
// either side of the slash or the plus pushes them apart, since each keycap
// already carries the padding of its own frame.
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

	void renderText(const std::string& text, const Vec2i& position, const Vec4d& color);
	void renderTextPure(const std::string& text);
	// p_outCharPositions gets one position per byte of the text and one behind
	// it, always text.length() + 1 of them - including the bytes of <h> and
	// </h>, which draw nothing themselves. The edit boxes look up here under
	// the same byte index their caret sits at.
	void measureText(const std::string& text, Vec2i* p_outDimensions, std::vector<Vec2i>* p_outCharPositions, const Vec2i& offset = Vec2i(0, 0));

	// Bring the text down to at most maxWidth pixels: what no longer fits is
	// dropped and replaced by three dots. renderText() neither clips nor wraps
	// of its own accord - text written to a fixed place has to be passed
	// through here first.
	std::string fitText(const std::string& text, int maxWidth);
	std::string adjustText(const std::string& text, int maxWidth);

	int getLineHeight() const;

	const Options& getOptions() const;
	void setOptions(const Options& options);
	void pushOptions();
	void popOptions();

	Texture* getTexture();

private:
	struct CharacterInfo
	{
		Vec2i position;
		Vec2i size;
	};

	struct StringCacheEntry
	{
		uint lastTimeUsed;
		uint listIndex;
	};

	Font(const std::string& filename);
	~Font();

	static bool forceReload() { return false; }

	// What one character advances the cursor by, before the character spacing:
	// its own glyph, or half a space's for the half space, which has no glyph.
	int getCharacterWidth(unsigned char c) const;

	// The rows a keycap frame occupies inside a glyph cell, as (top, height).
	Vec2i getKeyBoxRows() const;

	int lineHeight;
	int offset;

	// The rows of a glyph cell a keycap frame runs from and to, from the font's
	// capTop/capBottom attributes and otherwise the line box. lineHeight and
	// offset do not say this: a font may hang its line lower than its ink, and
	// a small font's letters may not fit inside its line at all.
	int capTop;
	int capBottom;

	CharacterInfo charInfo[256];
	Texture* p_texture;
	Options options;
	uint listBase;
	uint numLists;
	uint listFree;
	std::unordered_map<std::string, StringCacheEntry> stringCache;
	std::stack<Options> optionsStack;
};

#endif