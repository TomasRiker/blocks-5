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

	// One corner of a glyph quad: a position and a texture coordinate, and no
	// colour. That is what lets one built string serve all three passes
	// renderText() makes over it - two shadow samples and the text - under
	// nothing but a different glColor and a different translate. Float and not
	// int, because GL_INT is not a valid vertex attribute type in WebGL/GLES2,
	// and a glyph's pixels are well inside float's exact range.
	struct Vertex
	{
		Vertex(double px, double py, double u, double v)
			: position(static_cast<float>(px), static_cast<float>(py)),
			  uv(static_cast<float>(u), static_cast<float>(v)) {}

		Vec2f position;
		Vec2f uv;
	};

	// A laid-out string, ready to draw. The keycap frames are a batch of their
	// own because they carry no texture at all - four thin quads to a frame.
	struct StringCacheEntry
	{
		uint lastTimeUsed;
		std::vector<Vertex> glyphs;
		std::vector<Vec2f> keyBoxes;
	};

	Font(const std::string& filename);
	~Font();

	static bool forceReload() { return false; }

	// What one character advances the cursor by, before the character spacing:
	// its own glyph, or half a space's for the half space, which has no glyph.
	int getCharacterWidth(unsigned char c) const;

	// The rows a keycap frame occupies inside a glyph cell, as (top, height).
	Vec2i getKeyBoxRows() const;

	// The laid-out string for this text under the options in force, built on
	// the first ask and kept until it is the oldest entry in the cache.
	const StringCacheEntry& lookUpText(const std::string& text);
	void buildText(const std::string& text, std::vector<Vertex>& glyphs, std::vector<Vec2f>& keyBoxes);
	void drawText(const StringCacheEntry& entry) const;

	// The key that entry sits under: the text, with every option the layout
	// depends on in front of it. Returns a reference into cacheKeyBuffer.
	const std::string& cacheKey(const std::string& text);

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
	std::unordered_map<std::string, StringCacheEntry> stringCache;
	// Reused rather than returned by value: cacheKey() runs for every string
	// drawn in every frame, and a key built into a fresh string would be an
	// allocation each time.
	std::string cacheKeyBuffer;
	std::stack<Options> optionsStack;
};

#endif