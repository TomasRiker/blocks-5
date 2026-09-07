#include "pch.h"
#include "font.h"
#include "filesystem.h"
#include "texture.h"

// The <k> box - a keycap drawn around a key's name, so that "press Esc" reads
// as a key and not as a word. The padding keeps the frame off the glyphs
// inside it and the gap keeps it off the words either side; without the second
// the frame touches both neighbours. Both are in the font's own pixels, so a
// keycap in the tooltip font comes out proportionally smaller.
const int KEY_BOX_PAD = 3;
const int KEY_BOX_GAP = 2;

// What one side of a box costs the line, frame included.
const int KEY_BOX_SIDE = KEY_BOX_GAP + KEY_BOX_PAD;

// How far the frame stands off the line box. It is anchored there and not to
// the glyph cell: every glyph in a font has the same height, but that cell is
// taller than the line and a frame hung off it would float below the text.
const int KEY_BOX_GROW = 1;

namespace
{
	// Close the innermost keycap still open: its frame runs from the left edge
	// that was remembered when it opened to wherever the cursor stands now.
	void closeKeyBox(std::vector<Vec4i>& boxes,
					 std::vector<Vec2i>& open,
					 int cursorX,
					 int lineHeight)
	{
		if(open.empty()) return;

		const Vec2i start = open.back();
		open.pop_back();
		boxes.push_back(Vec4i(start.x,
							  start.y - KEY_BOX_GROW,
							  cursorX + KEY_BOX_PAD,
							  start.y + lineHeight + KEY_BOX_GROW));
	}
}

Font::Font(const std::string& filename) : Resource(filename)
{
	p_texture = 0;
	listBase = 0;

	// default options
	options.tabSize = 80;
	options.charSpacing = 0;
	options.lineSpacing = 1.0;
	options.charScaling = 1.0;
	options.shadows = 2;
	options.italic = 0;

	// generate the lists
	numLists = 32;
	listBase = glGenLists(numLists);
	listFree = ~0;

	reload();
}

Font::~Font()
{
	if(listBase)
	{
		// delete the lists
		glDeleteLists(listBase, numLists);
		listBase = 0;
	}

	cleanUp();
}

void Font::reload()
{
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

	TiXmlHandle docHandle(&doc);
	TiXmlHandle fontHandle = docHandle.FirstChildElement("Font");
	TiXmlElement* p_fontElement = fontHandle.Element();

	// read the image filename, line height and offset
	const char* p_imageFilename = p_fontElement->Attribute("image");
	p_fontElement->Attribute("lineHeight", &lineHeight);
	p_fontElement->Attribute("offset", &offset);

	// process all child elements
	TiXmlElement* p_charElement = p_fontElement->FirstChildElement("Character");
	while(p_charElement)
	{
		int code, x, y, w, h;
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

	// clear the cache
	stringCache.clear();
	listFree = ~0;
}

void Font::cleanUp()
{
	if(p_texture)
	{
		// release the texture
		p_texture->release();
		p_texture = 0;
	}
}

void Font::renderText(const std::string& text,
					  const Vec2i& position,
					  const Vec4d& color)
{
	bool cached;
	uint listIndex;

	// Is this string already in the cache?
	std::unordered_map<std::string, StringCacheEntry>::iterator entry = stringCache.find(text);
	if(entry != stringCache.end())
	{
		// Yes, already in the cache!
		listIndex = entry->second.listIndex;
		entry->second.lastTimeUsed = SDL_GetTicks();
		cached = true;
	}
	else
	{
		// The string has to be generated afresh because it is not in the
		// cache. Is there still room in the cache?
		if(stringCache.size() < numLists)
		{
			// Yes, there is still room. Find a free list!
			listIndex = 0;
			while(!(listFree & (1 << listIndex))) listIndex++;

			// This list is taken now.
			listFree &= ~(1 << listIndex);
		}
		else
		{
			// The oldest entry gets overwritten.
			uint minTime = ~0;
			std::unordered_map<std::string, StringCacheEntry>::iterator oldestEntry;
			for(std::unordered_map<std::string, StringCacheEntry>::iterator i = stringCache.begin(); i != stringCache.end(); ++i)
			{
				if(i->second.lastTimeUsed < minTime)
				{
					minTime = i->second.lastTimeUsed;
					oldestEntry = i;
				}
			}

			// remember the list index and delete the entry
			listIndex = oldestEntry->second.listIndex;
			stringCache.erase(oldestEntry);
		}

		// create the new entry
		StringCacheEntry newEntry;
		newEntry.lastTimeUsed = SDL_GetTicks();
		newEntry.listIndex = listIndex;
		stringCache[text] = newEntry;

		cached = false;
	}

#ifndef __EMSCRIPTEN__
	if(!cached)
	{
		glNewList(listBase + listIndex, GL_COMPILE);
		renderTextPure(text);
		glEndList();
	}
#endif

	glPushMatrix();
	glTranslated(position.x, position.y, 0.0);

	// draw the shadow if wanted
	if(options.shadows)
	{
		std::vector<Vec2i> samples;
		if(options.shadows == 2) { samples.push_back(Vec2i(2, 1)); samples.push_back(Vec2i(1, 2)); }
		else if(options.shadows == 1) { samples.push_back(Vec2i(1, 0)); samples.push_back(Vec2i(0, 1)); }
		int numSamples = static_cast<int>(samples.size());
		Vec4d shadowColor(0.0, 0.0, 0.0, 0.7 / numSamples);
		shadowColor.a *= color.a;

		for(int i = 0; i < numSamples; i++)
		{
			glColor4dv(shadowColor);
			glPushMatrix();
			glTranslated(samples[i].x, samples[i].y, 0.0);
#ifdef __EMSCRIPTEN__
			renderTextPure(text);   // no display lists in WebGL - draw it again
#else
			glCallList(listBase + listIndex);
#endif
			glPopMatrix();
		}
	}

	// draw the string
	glColor4dv(color);
#ifdef __EMSCRIPTEN__
	renderTextPure(text);
#else
	glCallList(listBase + listIndex);
#endif

	glPopMatrix();
}

void Font::renderTextPure(const std::string& text)
{
	p_texture->bind();

	glBegin(GL_QUADS);

	Vec2i cursor(0, offset);

	// The options stack belongs to the font and not to the text, but <h> holds
	// only within this one string - renderText() lays down one display list per
	// string, and an <h> could therefore not act across that boundary at all.
	// The counter keeps the two apart: without it a missing </h> would turn
	// every further text in the game italic, and an extra one would pop the
	// caller's own saved options or reach into an empty stack.
	size_t openTags = 0;

	// A keycap frame carries no texture and so cannot go into the glyph batch.
	// The rectangles are collected here and drawn once the batch is closed,
	// which also puts them through the shadow passes with the text - a keycap
	// without the same shadow would look pasted on. openBoxes holds the left
	// edge and the line top of every <k> not yet closed.
	std::vector<Vec4i> boxes;
	std::vector<Vec2i> openBoxes;

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
			openBoxes.push_back(Vec2i(cursor.x - KEY_BOX_PAD, cursor.y - offset));
			i += 2;
		}
		else if(r >= 3 && text[i] == '<' && text[i + 1] == '/' && text[i + 2] == 'k' && text[i + 3] == '>')
		{
			closeKeyBox(boxes, openBoxes, cursor.x, lineHeight);
			cursor.x += KEY_BOX_SIDE;
			i += 3;
		}
		else
		{
			const CharacterInfo& info = charInfo[c];

			if(options.charScaling == 1.0)
			{
				glTexCoord2i(info.position.x, info.position.y);
				glVertex2i(cursor.x + options.italic, cursor.y);
				glTexCoord2i(info.position.x + info.size.x, info.position.y);
				glVertex2i(cursor.x + info.size.x + options.italic, cursor.y);
				glTexCoord2i(info.position.x + info.size.x, info.position.y + info.size.y);
				glVertex2i(cursor.x + info.size.x, cursor.y + info.size.y);
				glTexCoord2i(info.position.x, info.position.y + info.size.y);
				glVertex2i(cursor.x, cursor.y + info.size.y);
			}
			else
			{
				glTexCoord2i(info.position.x, info.position.y);
				glVertex2d(cursor.x + options.italic, cursor.y);
				glTexCoord2i(info.position.x + info.size.x, info.position.y);
				glVertex2d(cursor.x + options.charScaling *info.size.x + options.italic, cursor.y);
				glTexCoord2i(info.position.x + info.size.x, info.position.y + info.size.y);
				glVertex2d(cursor.x + options.charScaling *info.size.x, cursor.y + options.charScaling *info.size.y);
				glTexCoord2i(info.position.x, info.position.y + info.size.y);
				glVertex2d(cursor.x, cursor.y + options.charScaling *info.size.y);
			}

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
	while(!openBoxes.empty()) closeKeyBox(boxes, openBoxes, cursor.x, lineHeight);

	glEnd();
	p_texture->unbind();

	// Untextured, and only now: unbind() has just switched texturing off. Four
	// thin quads and not a line loop, because a line's pixel coverage is a
	// matter of the rasterizer's opinion and every other edge in this game sits
	// on whole pixels.
	if(!boxes.empty())
	{
		glBegin(GL_QUADS);
		for(size_t b = 0; b < boxes.size(); b++)
		{
			const Vec4i& r = boxes[b];
			const int edges[4][4] = {{r.x, r.y, r.z, r.y + 1},          // top
									 {r.x, r.w - 1, r.z, r.w},          // bottom
									 {r.x, r.y, r.x + 1, r.w},          // left
									 {r.z - 1, r.y, r.z, r.w}};         // right
			for(int e = 0; e < 4; e++)
			{
				glVertex2i(edges[e][0], edges[e][1]);
				glVertex2i(edges[e][2], edges[e][1]);
				glVertex2i(edges[e][2], edges[e][3]);
				glVertex2i(edges[e][0], edges[e][3]);
			}
		}
		glEnd();
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

	// The start of the text up to byte n, then the three dots. A half-cut
	// element drops entirely and anything left open is closed again: <h>
	// pushes something onto a stack that only </h> takes off, and that stack
	// belongs to the font and not to the text - a cut-off <h> would turn every
	// further text in the game italic. The open ones are remembered by name
	// and closed in reverse, since <k> can stand inside <h>.
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
	Vec2i dim;
	measureText(text, &dim, 0);
	if(dim.x <= maxWidth) return text;

	// Binary search over the length, measuring the whole candidate including
	// the dots every time. The character positions from measureText() would
	// give the place in one pass, but not the width: that is not cursor.x but
	// the maximum over cursor.x + character width + italic slant, a line break
	// resets cursor.x anyway, and how wide the three dots come out depends on
	// whether an <h> is open at the cut. Measuring the whole candidate asks
	// exactly what the answer is supposed to be.
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
	Vec2d cursor(0, 0);
	Vec2d maximum(0, lineHeight);

	// As in renderTextPure(): <h> ends with this text at the latest.
	size_t openTags = 0;

	for(size_t i = 0; i < text.length(); i++)
	{
		uint r = static_cast<uint>(text.length() - i - 1);

		// One position per byte, not per pass. The edit boxes look up here
		// under the same byte index their caret sits at, and an <h> is three
		// bytes long in a single pass: the loop therefore first fills in what
		// the previous pass skipped. The cursor still stands exactly where it
		// did before the element, which has no width - a caret inside an <h>
		// stands at the place the element does.
		if(p_outCharPositions)
			while(p_outCharPositions->size() <= i) p_outCharPositions->push_back(cursor + offset);

		unsigned char c = text[i];
		if(c == '\n' || static_cast<char>(c) == '\xB6')
		{
			// line break
			cursor.x = 0;
			cursor.y += static_cast<double>(lineHeight) * options.lineSpacing;
			maximum.y = max(maximum.y, cursor.y + lineHeight);
		}
		else if(c == '\t')
		{
			cursor.x += options.tabSize;
			cursor.x /= options.tabSize;
			cursor.x *= options.tabSize;
			maximum.x = max(maximum.x, cursor.x);
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
			// Exactly what renderTextPure() advances by, or a keycap would be
			// drawn wider than it was measured.
			cursor.x += KEY_BOX_SIDE;
			maximum.x = max(maximum.x, cursor.x);
			i += 2;
		}
		else if(r >= 3 && text[i] == '<' && text[i + 1] == '/' && text[i + 2] == 'k' && text[i + 3] == '>')
		{
			cursor.x += KEY_BOX_SIDE;
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
}

std::string Font::adjustText(const std::string& text,
							 int maxWidth)
{
	int cursorX = 0;
	std::string out;

	for(size_t i = 0; i < text.length(); i++)
	{
		// A keycap is one atom. The frame around it is a box, and a box cannot
		// be broken across two lines, so the whole run moves down together -
		// which is what any typesetter does with an inline box and what keeps
		// the renderer from ever having to draw half a frame. It is measured
		// here rather than walked character by character because the padding
		// either side belongs to its width.
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
				const size_t lastBreak = out.find_last_of(" \n\xB6");
				if(lastBreak != std::string::npos && out[lastBreak] == ' ')
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

		// <h> and </h> draw nothing: they do not count toward the line width
		// and pass through untouched. A hard break would otherwise cut right
		// into one and turn the element into visible text - "<h>Kopf</h>" would
		// come out as "<h>Kopf<" and "/h>".
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
		else
		{
			const CharacterInfo& info = charInfo[c];
			int currentWidth = cursorX + info.size.x;

			if(currentWidth > maxWidth)
			{
				// replace the last space in this line with a line break
				int back = 0;
				std::string::reverse_iterator j;
				for(j = out.rbegin(); j != out.rend(); j++)
				{
					unsigned char d = *j;
					if(d == '\n' || static_cast<char>(d) == '\xB6')
					{
						j = out.rend();
						break;
					}
					else if(d == ' ')
					{
						*j = '\n';
						cursorX = back;
						break;
					}
					else
					{
						// Backwards too, an element does not count.
						// out.rend() - j is the index behind it,
						// because rend() - rbegin() is the text length.
						const size_t back_tag = (d == '>')
							? tagEndingAt(out, static_cast<size_t>(out.rend() - j)) : 0;
						if(back_tag > 0) j += back_tag - 1;
						else back += charInfo[d].size.x + options.charSpacing;
					}
				}

				if(j == out.rend())
				{
					// brutal line break
					out[out.length() - 1] = '\n';
					out.append(1, c);
					cursorX = info.size.x + options.charSpacing;
				}
			}
			else
			{
				cursorX += info.size.x + options.charSpacing;
			}
		}
	}

	return out;
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
	if(this->options.tabSize != options.tabSize ||
	   this->options.charSpacing != options.charSpacing ||
	   this->options.lineSpacing != options.lineSpacing ||
	   this->options.charScaling != options.charScaling ||
	   this->options.italic != options.italic)
	{
		// The cache is invalid now!
		stringCache.clear();
		listFree = ~0;
	}

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