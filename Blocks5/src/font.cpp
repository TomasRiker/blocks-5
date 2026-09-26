#include "pch.h"
#include "font.h"
#include "filesystem.h"
#include "texture.h"
#include "engine.h"

// The <k> keycap, a frame drawn around a key's name. The padding keeps the
// frame off the glyphs inside it, the gap off the words either side. Both are
// in the font's own pixels, so a keycap in the tooltip font comes out
// proportionally smaller.
const int KEY_BOX_PAD = 3;
const int KEY_BOX_GAP = 2;

// What one side of a keycap costs the line, frame included. The right side
// costs options.italic more: an italic glyph's top leans that far right of its
// foot, so a frame ending at the cursor would cut the last letter. The left
// side needs nothing, since the first letter's foot stands on the cursor.
const int KEY_BOX_SIDE = KEY_BOX_GAP + KEY_BOX_PAD;

// The geometry cache's budget, in quads and shared by every font. A quad is
// four 16-byte QuadVertex, so this is 512 KB, against a measured worst case of
// 825 quads with every screen the game has visited still cached. A ceiling and
// not a target: a screen over it would simply rebuild its oldest string.
const size_t QUAD_BUDGET = 8192;

// The dimensions cache's budget, in bytes of key and entry, shared the same
// way. The help page, the most text the game wraps at once, measured 22
// entries and 1.1 KB. A ceiling for the one case that could grow without one:
// stepping through a campaign measures fresh fitText() candidates per level,
// and a folder of single levels has no promised length.
const size_t DIM_BUDGET = 65536;

Font::CacheStats Font::cacheStats = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
std::vector<Font*> Font::liveFonts;
uint Font::lruClock = 0;

size_t Font::entryQuads(const StringCacheEntry& entry)
{
	// Four vertices to a quad in both arrays.
	return (entry.glyphs.size() + entry.keyBoxes.size()) / 4;
}

void Font::resetCacheStats()
{
	// The seven counters and not the four sizes: entries and quads are what is
	// standing right now, so clearing them would report a cache that is full
	// as empty until the next miss.
	cacheStats.hits = 0;
	cacheStats.misses = 0;
	cacheStats.evictions = 0;
	cacheStats.measures = 0;
	cacheStats.measureHits = 0;
	cacheStats.dimHits = 0;
	cacheStats.dimEvictions = 0;
}

namespace
{
	// Where a line may be broken. The first two are replaced by the break and
	// the other two already are one, which is why the caller has to tell them
	// apart after searching for the last of them.
	const char BREAK_CHARACTERS[] = { ' ', static_cast<char>(HALF_SPACE), '\n', '\xB6', 0 };

	bool isBreakSpace(unsigned char c)
	{
		return c == ' ' || c == HALF_SPACE;
	}

	// One value's bytes, appended to a cache key.
	template<typename T> void appendRaw(std::string& key, const T& value)
	{
		key.append(reinterpret_cast<const char*>(&value), sizeof(value));
	}

	// Close the innermost keycap still open: its frame runs from the left edge
	// that was remembered when it opened to wherever the cursor stands now.
	void closeKeyBox(std::vector<Vec4i>& boxes,
					 std::vector<Vec2i>& open,
					 int cursorX,
					 int height,
					 int italic)
	{
		if(open.empty()) return;

		const Vec2i start = open.back();
		open.pop_back();
		boxes.push_back(Vec4i(start.x,
							  start.y,
							  cursorX + KEY_BOX_PAD + italic,
							  start.y + height));
	}
}

Font::Font(const std::string& filename, int) : Resource(filename)
{
	liveFonts.push_back(this);

	p_texture = 0;
	lineHeight = 0;
	offset = 0;
	capTop = 0;
	capBottom = 0;

	// default options
	options.tabSize = 80;
	options.charSpacing = 0;
	options.lineSpacing = 1.0f;
	options.charScaling = 1.0f;
	options.shadows = 2;
	options.italic = 0;

	reload();
}

Font::~Font()
{
	cleanUp();

	for(std::vector<Font*>::iterator i = liveFonts.begin(); i != liveFonts.end(); ++i)
	{
		if(*i == this) { liveFonts.erase(i); break; }
	}
}

bool Font::makeRoom(size_t quads)
{
	if(quads > QUAD_BUDGET) return false;

	while(cacheStats.quads + quads > QUAD_BUDGET)
	{
		// The oldest of every font's entries, not of this one's: a font that
		// is barely used must not go on holding what a busy one needs.
		Font* p_oldestFont = 0;
		std::unordered_map<std::string, StringCacheEntry>::iterator oldest;
		uint oldestUsed = ~0u;

		for(std::vector<Font*>::iterator f = liveFonts.begin(); f != liveFonts.end(); ++f)
		{
			for(std::unordered_map<std::string, StringCacheEntry>::iterator i = (*f)->stringCache.begin();
				i != (*f)->stringCache.end(); ++i)
			{
				if(i->second.lastUsed < oldestUsed)
				{
					oldestUsed = i->second.lastUsed;
					oldest = i;
					p_oldestFont = *f;
				}
			}
		}

		// Cannot happen after the guard above, but a loop that could spin for
		// ever is worth one comparison.
		if(!p_oldestFont) return false;

		cacheStats.evictions++;
		cacheStats.entries--;
		cacheStats.quads -= entryQuads(oldest->second);
		p_oldestFont->stringCache.erase(oldest);
	}

	return true;
}

size_t Font::dimEntryBytes(const std::string& key)
{
	// The key and the entry. The map's own per-node overhead is left out,
	// since nothing here can name it.
	return key.length() + sizeof(DimCacheEntry);
}

bool Font::makeDimRoom(size_t bytes)
{
	if(bytes > DIM_BUDGET) return false;

	while(cacheStats.dimBytes + bytes > DIM_BUDGET)
	{
		// The oldest of every font's, off the same clock the geometry cache
		// ticks: one notion of which entry has gone longest unused.
		Font* p_oldestFont = 0;
		std::unordered_map<std::string, DimCacheEntry>::iterator oldest;
		uint oldestUsed = ~0u;

		for(std::vector<Font*>::iterator f = liveFonts.begin(); f != liveFonts.end(); ++f)
		{
			for(std::unordered_map<std::string, DimCacheEntry>::iterator i = (*f)->dimCache.begin();
				i != (*f)->dimCache.end(); ++i)
			{
				if(i->second.lastUsed < oldestUsed)
				{
					oldestUsed = i->second.lastUsed;
					oldest = i;
					p_oldestFont = *f;
				}
			}
		}

		if(!p_oldestFont) return false;

		cacheStats.dimEvictions++;
		cacheStats.dimEntries--;
		cacheStats.dimBytes -= dimEntryBytes(oldest->first);
		p_oldestFont->dimCache.erase(oldest);
	}

	return true;
}

void Font::rememberDimensions(const std::string& text, const Vec2i& dimensions)
{
	const std::string& key = cacheKey(text);
	const size_t bytes = dimEntryBytes(key);

	// makeDimRoom() erases from these maps and builds no key of its own, so
	// the reference above still stands under it.
	if(!makeDimRoom(bytes)) return;

	DimCacheEntry& created = dimCache[key];
	created.lastUsed = ++lruClock;
	created.dimensions = dimensions;
	cacheStats.dimEntries++;
	cacheStats.dimBytes += bytes;
}

void Font::forgetDimensions(const std::string& key)
{
	std::unordered_map<std::string, DimCacheEntry>::iterator i = dimCache.find(key);
	if(i == dimCache.end()) return;

	cacheStats.dimEntries--;
	cacheStats.dimBytes -= dimEntryBytes(key);
	dimCache.erase(i);
}

void Font::reload()
{
	// This drops the caches too: the glyph rectangles may move, and every
	// laid-out string with them.
	cleanUp();

	// load the XML document
	std::string text = FileSystem::inst().readStringFromFile(filename);
	TiXmlDocument doc;
	doc.Parse(text.c_str());
	if(doc.ErrorId())
	{
		printfLog("+ ERROR: Could not parse font XML file \"%s\" (Error: %d).\n",
				  filename.c_str(),
				  doc.ErrorId());
		error = 1;
		return;
	}

	// A skin brings its note's font, and a skin can come from anybody, so a
	// document without its <Font> or its image is a broken file rather than a
	// null pointer to walk into.
	TiXmlHandle docHandle(&doc);
	TiXmlHandle fontHandle = docHandle.FirstChildElement("Font");
	TiXmlElement* p_fontElement = fontHandle.Element();
	const char* p_imageFilename = p_fontElement ? p_fontElement->Attribute("image") : 0;
	if(!p_imageFilename)
	{
		printfLog("+ ERROR: Font XML file \"%s\" has no <Font> element with an image.\n",
				  filename.c_str());
		error = 3;
		return;
	}

	// read the line height and offset
	p_fontElement->Attribute("lineHeight", &lineHeight);
	p_fontElement->Attribute("offset", &offset);

	// The rows of a glyph cell a keycap frame runs over: the row a capital
	// begins at and the row the writing ends on. Both are optional and
	// default to the line box, where font.xml and credits_font.xml have their
	// ink; the note's font hangs its line lower than its writing and has to
	// say so. verify.py's font_metrics check reads both out of the image and
	// reports figures that do not describe it.
	capTop = -offset;
	capBottom = -offset + lineHeight - 1;
	p_fontElement->Attribute("capTop", &capTop);
	p_fontElement->Attribute("capBottom", &capBottom);

	// Process all child elements. A character the file leaves out has no
	// glyph, whatever a previous load of it said, and one that leaves out an
	// attribute gets 0 rather than whatever the stack held.
	for(int i = 0; i < 256; i++) charInfo[i].position = charInfo[i].size = Vec2i(0, 0);
	TiXmlElement* p_charElement = p_fontElement->FirstChildElement("Character");
	while(p_charElement)
	{
		int code = -1, x = 0, y = 0, w = 0, h = 0;
		p_charElement->Attribute("code", &code);
		p_charElement->Attribute("x", &x);
		p_charElement->Attribute("y", &y);
		p_charElement->Attribute("w", &w);
		p_charElement->Attribute("h", &h);

		if(code >= 0 && code < 256)
		{
			charInfo[code].position = Vec2i(x, y);
			charInfo[code].size = Vec2i(w, h);
		}

		p_charElement = p_charElement->NextSiblingElement("Character");
	}

	// load the texture
	std::string dir = FileSystem::inst().getPathDirectory(filename);
	std::string imageFilename = dir + (dir.empty() ? "" : "/") + std::string(p_imageFilename);
	p_texture = Manager<Texture>::inst().request(imageFilename);
	if(!p_texture)
	{
		printfLog("+ ERROR: Could not load font texture \"%s\" for font \"%s\".\n",
				  p_imageFilename,
				  filename.c_str());
		error = 2;
		return;
	}
}

void Font::dropCache()
{
	// In cleanUp(), so that a reload and a destruction both come through here.

	for(std::unordered_map<std::string, StringCacheEntry>::const_iterator i = stringCache.begin();
		i != stringCache.end(); ++i)
	{
		cacheStats.entries--;
		cacheStats.quads -= entryQuads(i->second);
	}
	stringCache.clear();

	for(std::unordered_map<std::string, DimCacheEntry>::const_iterator i = dimCache.begin();
		i != dimCache.end(); ++i)
	{
		cacheStats.dimEntries--;
		cacheStats.dimBytes -= dimEntryBytes(i->first);
	}
	dimCache.clear();
}

void Font::cleanUp()
{
	dropCache();

	if(p_texture)
	{
		// release the texture
		p_texture->release();
		p_texture = 0;
	}
}

void Font::renderText(const std::string& text,
					  const Vec2i& position,
					  const Vec4f& color,
					  bool cache)
{
	// cache=false is for a string whose layout will not be asked for again,
	// like the credits' ending, whose animated charScaling makes a fresh key
	// every frame (measured over six seconds: 0 of 244 lookups hit, 212
	// evictions). A parameter and not part of the key, which would hold two
	// copies of a string asked for both ways.
	const StringCacheEntry& entry = lookUpText(text, cache);

	Renderer& renderer = Renderer::inst();
	renderer.push();
	renderer.translate(static_cast<float>(position.x), static_cast<float>(position.y));

	// draw the shadow if wanted
	if(options.shadows)
	{
		std::vector<Vec2i> samples;
		if(options.shadows == 2) { samples.push_back(Vec2i(2, 1)); samples.push_back(Vec2i(1, 2)); }
		else if(options.shadows == 1) { samples.push_back(Vec2i(1, 0)); samples.push_back(Vec2i(0, 1)); }
		int numSamples = static_cast<int>(samples.size());
		Vec4f shadowColor(0.0f, 0.0f, 0.0f, 0.7f / numSamples);
		shadowColor.a *= color.a;

		for(int i = 0; i < numSamples; i++)
		{
			renderer.push();
			renderer.translate(static_cast<float>(samples[i].x), static_cast<float>(samples[i].y));
			drawText(entry, shadowColor);
			renderer.pop();
		}
	}

	// draw the string
	drawText(entry, color);

	renderer.pop();
}

const std::string& Font::cacheKey(const std::string& text)
{
	// The options go in one at a time rather than as the struct's bytes, whose
	// padding is indeterminate and would let two equal option sets hash apart.
	// shadows is left out: the shadow is the same arrays drawn again at an
	// offset, so it changes nothing that is built.
	cacheKeyBuffer.clear();
	appendRaw(cacheKeyBuffer, options.tabSize);
	appendRaw(cacheKeyBuffer, options.charSpacing);
	appendRaw(cacheKeyBuffer, options.lineSpacing);
	appendRaw(cacheKeyBuffer, options.charScaling);
	appendRaw(cacheKeyBuffer, options.italic);
	cacheKeyBuffer += text;
	return cacheKeyBuffer;
}

const Font::StringCacheEntry& Font::lookUpText(const std::string& text, bool cache)
{
	if(cache)
	{
		std::unordered_map<std::string, StringCacheEntry>::iterator entry = stringCache.find(cacheKey(text));
		if(entry != stringCache.end())
		{
			cacheStats.hits++;
			entry->second.lastUsed = ++lruClock;
			return entry->second;
		}

		cacheStats.misses++;

		// A copy, where the hit above used the reference cacheKey() returns:
		// the measureText() below builds a key of its own into that same
		// buffer, so nothing held across it survives.
		const std::string key = cacheKey(text);

		// Laid out into the scratch entry to learn what it costs, and swapped
		// into the cache if the budget can be made to hold it.
		buildText(text, scratchEntry.glyphs, scratchEntry.keyBoxes);

		if(makeRoom(entryQuads(scratchEntry)))
		{
			// Measured before the insertion, because measureText() reads this
			// same cache: an entry already standing in it but not yet measured
			// would answer with whatever was in the field. A string drawn from
			// the scratch entry is not measured at all: it has no later
			// measure to save.
			Vec2i dimensions;
			measureText(text, &dimensions, 0);

			// The geometry entry answers every later measure of this string,
			// so a dimensions entry the walk above left behind is dead weight.
			forgetDimensions(key);

			// A reference into an unordered_map survives a rehash, and only
			// makeRoom() and dropCache() erase, so renderText() can hold this
			// one over its three draws.
			StringCacheEntry& created = stringCache[key];
			created.lastUsed = ++lruClock;
			created.dimensions = dimensions;
			created.glyphs.swap(scratchEntry.glyphs);
			created.keyBoxes.swap(scratchEntry.keyBoxes);
			cacheStats.entries++;
			cacheStats.quads += entryQuads(created);
			return created;
		}

		// Too big for the budget on its own. Drawn from the scratch entry,
		// like a string the caller asked not to keep.
		return scratchEntry;
	}

	cacheStats.misses++;
	buildText(text, scratchEntry.glyphs, scratchEntry.keyBoxes);
	return scratchEntry;
}

void Font::drawText(const StringCacheEntry& entry, const Vec4f& color) const
{
	Renderer& renderer = Renderer::inst();

	// The frames go up first, so that the letters are drawn over them: a
	// frame's top edge can lie on the row the capitals begin at (capTop), and
	// behind the glyphs the overlap does not show. Four thin quads to a frame
	// and not a line loop, because a line's pixel coverage is the rasterizer's
	// choice and every other edge in this game sits on whole pixels.
	if(!entry.keyBoxes.empty())
	{
		renderer.quads(&entry.keyBoxes[0], static_cast<uint>(entry.keyBoxes.size()), color);
	}

	// None where a reload failed - the editor's Refresh over a broken skin.
	if(!p_texture) return;
	renderer.setTexture(p_texture->ref());
	if(!entry.glyphs.empty())
	{
		renderer.quads(renderer.state(), &entry.glyphs[0], static_cast<uint>(entry.glyphs.size()), color);
	}
}

void Font::buildText(const std::string& text,
					 std::vector<QuadVertex>& glyphs,
					 std::vector<Vec2f>& keyBoxes)
{
	glyphs.clear();
	keyBoxes.clear();

	// One quad per byte at most: only a line break, a tab, a half space and
	// the markup emit none. The scratch vectors are fresh whenever the last
	// entry was kept, and a long string would otherwise grow through a dozen
	// reallocations.
	glyphs.reserve(text.length() * 4);

	Vec2i cursor(0, offset);

	// <h> holds only within this string, but it pushes onto the font's own
	// options stack. The counter keeps the two apart: without it a missing
	// </h> would leave every later text italic, and an extra one would pop the
	// caller's saved options or top() an empty stack.
	size_t openTags = 0;

	// A keycap frame is flat and so cannot join the glyph batch: the
	// rectangles are collected here, turned into quads after the walk, and
	// drawn by drawText() through the same shadow passes as the glyphs.
	// openBoxes holds the left edge and top row of every <k> still open.
	std::vector<Vec4i> boxes;
	std::vector<Vec2i> openBoxes;
	const Vec2i keyBoxRows = getKeyBoxRows();

	for(size_t i = 0; i < text.length(); i++)
	{
		uint r = static_cast<uint>(text.length() - i - 1);

		unsigned char c = text[i];
		if(c == '\n' || static_cast<char>(c) == '\xB6')
		{
			// line break
			cursor.x = 0;
			cursor.y += static_cast<int>(options.lineSpacing * lineHeight);
		}
		else if(c == '\t')
		{
			cursor.x += options.tabSize;
			cursor.x /= options.tabSize;
			cursor.x *= options.tabSize;
		}
		else if(c == HALF_SPACE)
		{
			cursor.x += getCharacterWidth(c) + options.charSpacing;
		}
		else if(r >= 2 && text[i] == '<' && text[i + 1] == 'h' && text[i + 2] == '>')
		{
			optionsStack.push(options);
			options.italic = 4;
			openTags++;
			i += 2;
		}
		else if(r >= 3 && text[i] == '<' && text[i + 1] == '/' && text[i + 2] == 'h' && text[i + 3] == '>')
		{
			if(openTags > 0)
			{
				options = optionsStack.top();
				optionsStack.pop();
				openTags--;
			}
			i += 3;
		}
		else if(r >= 2 && text[i] == '<' && text[i + 1] == 'k' && text[i + 2] == '>')
		{
			cursor.x += KEY_BOX_SIDE;
			openBoxes.push_back(Vec2i(cursor.x - KEY_BOX_PAD, cursor.y + keyBoxRows.x));
			i += 2;
		}
		else if(r >= 3 && text[i] == '<' && text[i + 1] == '/' && text[i + 2] == 'k' && text[i + 3] == '>')
		{
			closeKeyBox(boxes, openBoxes, cursor.x, keyBoxRows.y, options.italic);
			cursor.x += KEY_BOX_SIDE + options.italic;
			i += 3;
		}
		else
		{
			const CharacterInfo& info = charInfo[c];

			// Only the two top corners carry the italic lean: a slanted glyph
			// is drawn that many pixels further along at its top than at its
			// foot, while the cursor advances by the upright width.
			const float w = options.charScaling * info.size.x;
			const float h = options.charScaling * info.size.y;
			const float x = static_cast<float>(cursor.x), y = static_cast<float>(cursor.y);
			const float lean = static_cast<float>(options.italic);
			const Vec2i& t = info.position;
			const Vec2i& ts = info.size;

			// The glyph's texture edges are whole texels, added up as such.
			const float u0 = static_cast<float>(t.x), v0 = static_cast<float>(t.y);
			const float u1 = static_cast<float>(t.x + ts.x), v1 = static_cast<float>(t.y + ts.y);

			glyphs.push_back(QuadVertex(x + lean,     y,     u0, v0));
			glyphs.push_back(QuadVertex(x + w + lean, y,     u1, v0));
			glyphs.push_back(QuadVertex(x + w,        y + h, u1, v1));
			glyphs.push_back(QuadVertex(x,            y + h, u0, v1));

			// The advance is the unscaled width: the scaling stretches the
			// glyph and not the setting.
			cursor.x += info.size.x + options.charSpacing;
		}
	}

	// Whatever the text left open, it closes here.
	while(openTags > 0)
	{
		options = optionsStack.top();
		optionsStack.pop();
		openTags--;
	}
	while(!openBoxes.empty()) closeKeyBox(boxes, openBoxes, cursor.x, keyBoxRows.y, options.italic);

	// Four thin quads to a frame, and by now the frames are counted.
	keyBoxes.reserve(boxes.size() * 16);

	for(size_t b = 0; b < boxes.size(); b++)
	{
		const Vec4i& r = boxes[b];
		const int edges[4][4] = {{r.x, r.y, r.z, r.y + 1},          // top
								 {r.x, r.w - 1, r.z, r.w},          // bottom
								 {r.x, r.y, r.x + 1, r.w},          // left
								 {r.z - 1, r.y, r.z, r.w}};         // right
		for(int e = 0; e < 4; e++)
		{
			const float x0 = static_cast<float>(edges[e][0]);
			const float y0 = static_cast<float>(edges[e][1]);
			const float x1 = static_cast<float>(edges[e][2]);
			const float y1 = static_cast<float>(edges[e][3]);
			keyBoxes.push_back(Vec2f(x0, y0));
			keyBoxes.push_back(Vec2f(x1, y0));
			keyBoxes.push_back(Vec2f(x1, y1));
			keyBoxes.push_back(Vec2f(x0, y1));
		}
	}
}

namespace
{
	// Length of the markup element that begins at this position; 0 if none
	// begins there. The font knows two: <h>...</h> and <k>...</k>.
	size_t tagLength(const std::string& text, size_t position)
	{
		if(text.compare(position, 3, "<h>") == 0) return 3;
		if(text.compare(position, 4, "</h>") == 0) return 4;
		if(text.compare(position, 3, "<k>") == 0) return 3;
		if(text.compare(position, 4, "</k>") == 0) return 4;
		return 0;
	}

	// Length of the element that stops exactly before the byte "end"; 0 if
	// none ends there. The counterpart to tagLength() for looking backwards.
	size_t tagEndingAt(const std::string& text, size_t end)
	{
		if(end >= 4 && text.compare(end - 4, 4, "</h>") == 0) return 4;
		if(end >= 3 && text.compare(end - 3, 3, "<h>") == 0) return 3;
		if(end >= 4 && text.compare(end - 4, 4, "</k>") == 0) return 4;
		if(end >= 3 && text.compare(end - 3, 3, "<k>") == 0) return 3;
		return 0;
	}

	// The last of BREAK_CHARACTERS in text that is not inside a keycap: a key
	// name such as "Page Up" holds a space, and breaking there would cut the
	// frame in two. npos where there is none.
	size_t lastBreakOutsideKeycap(const std::string& text)
	{
		size_t found = std::string::npos;
		bool inKeycap = false;
		for(size_t i = 0; i < text.length(); i++)
		{
			if(text.compare(i, 3, "<k>") == 0) inKeycap = true;
			else if(text.compare(i, 4, "</k>") == 0) inKeycap = false;
			else if(!inKeycap && text[i] && strchr(BREAK_CHARACTERS, text[i])) found = i;
		}
		return found;
	}

	// The start of the text up to byte n, then the three dots. A half-cut
	// element is dropped, and whatever is left open is closed again - by name
	// and innermost first, since <k> can stand inside <h> - because
	// measureText() counts a keycap's right side only at its </k>.
	std::string cutWithEllipsis(const std::string& text, size_t n)
	{
		std::string open;
		size_t i = 0;
		while(i < n)
		{
			const size_t length = tagLength(text, i);
			if(length == 0) { i++; continue; }
			if(i + length > n) break;
			if(length == 3) open += text[i + 1];
			else if(!open.empty()) open.erase(open.length() - 1);
			i += length;
		}

		std::string out = text.substr(0, i) + "...";
		while(!open.empty())
		{
			out += std::string("</") + open[open.length() - 1] + ">";
			open.erase(open.length() - 1);
		}
		return out;
	}
}

std::string Font::fitText(const std::string& text,
						  int maxWidth)
{
	// Down to at most maxWidth pixels, with what no longer fits replaced by
	// three dots.
	Vec2i dim;
	measureText(text, &dim, 0);
	if(dim.x <= maxWidth) return text;

	// Binary search over the length, measuring each whole candidate, dots
	// included. The character positions would find the cut in one pass but
	// not the width, which is the widest line's cursor plus glyph width plus
	// slant, and the dots come out wider where an <h> is open at the cut.
	size_t lo = 0, hi = text.length();
	while(lo < hi)
	{
		const size_t mid = (lo + hi + 1) / 2;
		measureText(cutWithEllipsis(text, mid), &dim, 0);
		if(dim.x <= maxWidth) lo = mid;
		else hi = mid - 1;
	}

	return cutWithEllipsis(text, lo);
}

void Font::measureText(const std::string& text,
					   Vec2i* p_outDimensions,
					   std::vector<Vec2i>* p_outCharPositions,
					   const Vec2i& offset)
{
	// Counted apart from the drawing's hits: fitText() measures once per
	// probe and adjustText() once per run and per line.
	cacheStats.measures++;

	// A string that has been drawn has already been walked, and its geometry
	// entry holds the answer: that covers every GUI widget, each of which
	// measures its caption in the onRender() that draws it. Only where
	// nothing but the size is wanted: the character positions depend on the
	// offset, which is no part of the key, and only the edit boxes ask.
	const bool sizeOnly = p_outDimensions && !p_outCharPositions;
	if(sizeOnly)
	{
		const std::string& key = cacheKey(text);

		std::unordered_map<std::string, StringCacheEntry>::iterator entry = stringCache.find(key);
		if(entry != stringCache.end())
		{
			cacheStats.measureHits++;
			entry->second.lastUsed = ++lruClock;
			*p_outDimensions = entry->second.dimensions;
			return;
		}

		// And the strings nothing draws: the runs and line tails adjustText()
		// measures and fitText()'s candidates, asked for again on every frame
		// the same text is wrapped or fitted. Nothing between the two lookups
		// builds a key, so the reference above is still this text's.
		std::unordered_map<std::string, DimCacheEntry>::iterator dim = dimCache.find(key);
		if(dim != dimCache.end())
		{
			cacheStats.dimHits++;
			dim->second.lastUsed = ++lruClock;
			*p_outDimensions = dim->second.dimensions;
			return;
		}
	}

	Vec2f cursor(0.0f, 0.0f);
	Vec2f maximum(0.0f, static_cast<float>(lineHeight));

	// As in buildText(): <h> ends with this text at the latest.
	size_t openTags = 0;

	for(size_t i = 0; i < text.length(); i++)
	{
		uint r = static_cast<uint>(text.length() - i - 1);

		// One position per byte, not per pass: the edit boxes index this by
		// their caret's byte, and a tag is consumed in a single pass. The
		// bytes a pass skipped get the cursor as the tag left it.
		if(p_outCharPositions)
			while(p_outCharPositions->size() <= i) p_outCharPositions->push_back(cursor + offset);

		unsigned char c = text[i];
		if(c == '\n' || static_cast<char>(c) == '\xB6')
		{
			// line break
			cursor.x = 0;
			cursor.y += static_cast<float>(lineHeight) * options.lineSpacing;
			maximum.y = max(maximum.y, cursor.y + lineHeight);
		}
		else if(c == '\t')
		{
			// The stop buildText() advances to. There the integer division on
			// a Vec2i snaps to a multiple of tabSize; on this float cursor the
			// same steps would cancel ((x + t) / t * t is x + t) and measure
			// the line wider than it is drawn.
			const int stop = (static_cast<int>(cursor.x) + options.tabSize) / options.tabSize;
			cursor.x = static_cast<float>(stop * options.tabSize);
			maximum.x = max(maximum.x, cursor.x);
		}
		else if(c == HALF_SPACE)
		{
			maximum.x = max(maximum.x, cursor.x + getCharacterWidth(c) + options.italic);
			maximum.y = max(maximum.y, cursor.y + lineHeight);

			cursor.x += getCharacterWidth(c) + options.charSpacing;
		}
		else if(r >= 2 && text[i] == '<' && text[i + 1] == 'h' && text[i + 2] == '>')
		{
			optionsStack.push(options);
			options.italic = 4;
			openTags++;
			i += 2;
		}
		else if(r >= 3 && text[i] == '<' && text[i + 1] == '/' && text[i + 2] == 'h' && text[i + 3] == '>')
		{
			if(openTags > 0)
			{
				options = optionsStack.top();
				optionsStack.pop();
				openTags--;
			}
			i += 3;
		}
		else if(r >= 2 && text[i] == '<' && text[i + 1] == 'k' && text[i + 2] == '>')
		{
			// Exactly what buildText() advances by, or a keycap would be
			// drawn wider than it was measured.
			cursor.x += KEY_BOX_SIDE;
			maximum.x = max(maximum.x, cursor.x);
			i += 2;
		}
		else if(r >= 3 && text[i] == '<' && text[i + 1] == '/' && text[i + 2] == 'k' && text[i + 3] == '>')
		{
			cursor.x += KEY_BOX_SIDE + options.italic;
			maximum.x = max(maximum.x, cursor.x);
			i += 3;
		}
		else
		{
			const CharacterInfo& info = charInfo[c];
			maximum.x = max(maximum.x, cursor.x + info.size.x + options.italic);
			maximum.y = max(maximum.y, cursor.y + lineHeight);

			cursor.x += info.size.x + options.charSpacing;
		}
	}

	// Whatever the text left open, it closes here.
	while(openTags > 0)
	{
		options = optionsStack.top();
		optionsStack.pop();
		openTags--;
	}

	// Up to and including text.length(), because the caret may stand behind
	// the last character too. The vector therefore always has exactly one
	// entry more than the text has bytes.
	if(p_outCharPositions)
		while(p_outCharPositions->size() <= text.length()) p_outCharPositions->push_back(cursor + offset);
	if(p_outDimensions) *p_outDimensions = maximum;
	if(sizeOnly) rememberDimensions(text, *p_outDimensions);
}

std::string Font::adjustText(const std::string& text,
							 int maxWidth)
{
	int cursorX = 0;
	std::string out;

	for(size_t i = 0; i < text.length(); i++)
	{
		// A keycap is one atom: its frame cannot be broken across two lines,
		// so the whole run moves down together. It is measured rather than
		// walked, because the padding either side belongs to its width.
		if(text.compare(i, 3, "<k>") == 0)
		{
			const size_t close = text.find("</k>", i);
			const size_t end = (close == std::string::npos) ? text.length() : close + 4;
			const std::string run = text.substr(i, end - i);

			Vec2i runDim;
			measureText(run, &runDim, 0);

			if(cursorX > 0 && cursorX + runDim.x > maxWidth)
			{
				// Break in front of it, at the last space of this line if there
				// is one. The tail is re-measured rather than counted
				// backwards, since it may hold a keycap of its own.
				const size_t lastBreak = lastBreakOutsideKeycap(out);
				if(lastBreak != std::string::npos && isBreakSpace(out[lastBreak]))
				{
					out[lastBreak] = '\n';
					Vec2i tailDim;
					measureText(out.substr(lastBreak + 1), &tailDim, 0);
					cursorX = tailDim.x;
				}
				else
				{
					out.append(1, '\n');
					cursorX = 0;
				}
			}

			out += run;
			cursorX += runDim.x;
			i = end - 1;
			continue;
		}

		// Markup draws nothing: it passes through untouched and does not count
		// toward the line width, and a hard break cutting into a tag would
		// turn it into visible text.
		const size_t tag = tagLength(text, i);
		if(tag > 0)
		{
			out.append(text, i, tag);
			i += tag - 1;
			continue;
		}

		unsigned char c = text[i];
		out.append(1, c);

		if(c == '\n' || static_cast<char>(c) == '\xB6')
		{
			// line break
			cursorX = 0;
		}
		else if(c == '\t')
		{
			// The stop buildText() draws to, not the width of charInfo['\t'],
			// which is 66 in font.xml.
			const int stop = (cursorX + options.tabSize) / options.tabSize;
			cursorX = stop * options.tabSize;
		}
		else
		{
			const int width = getCharacterWidth(c);
			int currentWidth = cursorX + width;

			if(currentWidth > maxWidth)
			{
				// Replace the last space in this line with a line break, but
				// not one inside a keycap, whose end the walk meets first.
				int back = 0;
				bool inKeycap = false;
				std::string::reverse_iterator j;
				for(j = out.rbegin(); j != out.rend(); j++)
				{
					unsigned char d = *j;
					if(d == '\n' || static_cast<char>(d) == '\xB6')
					{
						j = out.rend();
						break;
					}
					else if(isBreakSpace(d) && !inKeycap)
					{
						*j = '\n';
						cursorX = back;
						break;
					}
					else
					{
						// Backwards too, <h> draws nothing, while a keycap's
						// two tags stand for its frame and cost what
						// measureText() gives them.
						// out.rend() - j is the index behind it,
						// because rend() - rbegin() is the text length.
						const size_t behind = static_cast<size_t>(out.rend() - j);
						const size_t back_tag = (d == '>') ? tagEndingAt(out, behind) : 0;
						if(back_tag > 0)
						{
							if(out.compare(behind - back_tag, back_tag, "</k>") == 0)
							{
								inKeycap = true;
								back += KEY_BOX_SIDE + options.italic;
							}
							else if(out.compare(behind - back_tag, back_tag, "<k>") == 0)
							{
								inKeycap = false;
								back += KEY_BOX_SIDE;
							}
							j += back_tag - 1;
						}
						else back += getCharacterWidth(d) + options.charSpacing;
					}
				}

				if(j == out.rend())
				{
					// brutal line break
					out[out.length() - 1] = '\n';
					out.append(1, c);
					cursorX = width + options.charSpacing;
				}
			}
			else
			{
				cursorX += width + options.charSpacing;
			}
		}
	}

	return out;
}

Vec2i Font::getKeyBoxRows() const
{
	// capTop and capBottom are the frame itself, not a hint where to centre
	// one of the line's height: the note's font hangs its line five rows below
	// its writing, and the tooltip font's key names are taller than its
	// ten-row line, so a frame of the line's height fits neither.
	return Vec2i(capTop, max(1, capBottom - capTop + 1));
}

int Font::getCharacterWidth(unsigned char c) const
{
	// What one character advances the cursor by, before the character spacing
	// the caller adds on top. Half of this font's own space for the half
	// space, which has no glyph, so the gap keeps its proportion in the
	// tooltip font as well.
	if(c == HALF_SPACE) return charInfo[' '].size.x / 2;
	return charInfo[c].size.x;
}

int Font::getLineHeight() const
{
	return lineHeight;
}

const Font::Options& Font::getOptions() const
{
	return options;
}

void Font::setOptions(const Font::Options& options)
{
	// Nothing to invalidate: every option the layout depends on is part of the
	// cache key. Callers change options every frame - a speech balloon sets
	// italic and puts it back, the credits' ending animates charScaling - and
	// a cache emptied on each change would keep none of the GUI's strings.
	this->options = options;
}

void Font::pushOptions()
{
	optionsStack.push(options);
}

void Font::popOptions()
{
	if(!optionsStack.empty())
	{
		setOptions(optionsStack.top());
		optionsStack.pop();
	}
}

Texture* Font::getTexture()
{
	return p_texture;
}