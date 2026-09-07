#ifndef _FONT_H
#define _FONT_H

/*** Class for a font ***/

#include "resource.h"

class Texture;

// A space of half a space's width, and a space in every other respect: it is
// measured like one and a line breaks at one, replacing it exactly as a break
// replaces a space. It stands between the two keycaps of an action bound to
// two keys, where a full space either side of the slash pushes them apart -
// each keycap already carries the padding of its own frame.
//
// A byte rather than an element like <k>, because a break is a matter of
// characters: adjustText() looks backwards for the last one it may cut at, and
// an element would have to be taught to be a break as well as to be skipped.
// The byte is one no text can hold - the same idiom as the pilcrow that means
// a line break inside a localized string, only this one is never typed.
const char HALF_SPACE = '\x1F';

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

	// Read capTop and capBottom off the font's own image.
	void measureCapBox();

	// The rows a keycap frame occupies inside a glyph cell, as (top, height).
	// The height is the line's, so that keycaps on two lines above one another
	// share an edge rather than collide; the position is the letters', not the
	// line box's.
	Vec2i getKeyBoxRows(double lineSpacing) const;

	int lineHeight;
	int offset;

	// The row of a glyph cell that most letters begin at and the row most of
	// them end at - the mode over the printable characters, so that one deep
	// comma or one tall brace does not move the answer. lineHeight and offset
	// do not say this: a font may hang its line lower than its ink, and the
	// note's font does.
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