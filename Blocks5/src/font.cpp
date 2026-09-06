#include "pch.h"
#include "font.h"
#include "filesystem.h"
#include "texture.h"

Font::Font(const std::string& filename) : Resource(filename)
{
	p_texture = 0;
	listBase = 0;

	// Standardoptionen
	options.tabSize = 80;
	options.charSpacing = 0;
	options.lineSpacing = 1.0;
	options.charScaling = 1.0;
	options.shadows = 2;
	options.italic = 0;

	// Listen generieren
	numLists = 32;
	listBase = glGenLists(numLists);
	listFree = ~0;

	reload();
}

Font::~Font()
{
	if(listBase)
	{
		// Listen loeschen
		glDeleteLists(listBase, numLists);
		listBase = 0;
	}

	cleanUp();
}

void Font::reload()
{
	cleanUp();

	// XML-Dokument laden
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

	// Dateiname des Bilds, Zeilenhoehe und Offset lesen
	const char* p_imageFilename = p_fontElement->Attribute("image");
	p_fontElement->Attribute("lineHeight", &lineHeight);
	p_fontElement->Attribute("offset", &offset);

	// alle Kind-Elemente verarbeiten
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

	// Textur laden
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

	// Cache leeren
	stringCache.clear();
	listFree = ~0;
}

void Font::cleanUp()
{
	if(p_texture)
	{
		// Textur freigeben
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

	// Haben wir diesen String schon im Cache?
	std::unordered_map<std::string, StringCacheEntry>::iterator entry = stringCache.find(text);
	if(entry != stringCache.end())
	{
		// Ja, ist schon im Cache!
		listIndex = entry->second.listIndex;
		entry->second.lastTimeUsed = SDL_GetTicks();
		cached = true;
	}
	else
	{
		// Der String muss neu generiert werden, weil er nicht im Cache ist.
		// Ist noch Platz im Cache?
		if(stringCache.size() < numLists)
		{
			// Ja, es ist noch Platz. Freie Liste suchen!
			listIndex = 0;
			while(!(listFree & (1 << listIndex))) listIndex++;

			// Diese Liste ist jetzt belegt.
			listFree &= ~(1 << listIndex);
		}
		else
		{
			// Der aelteste Eintrag wird ueberschrieben.
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

			// Listenindex merken und den Eintrag loeschen
			listIndex = oldestEntry->second.listIndex;
			stringCache.erase(oldestEntry);
		}

		// neuen Eintrag erstellen
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

	// Schatten zeichnen, falls erwuenscht
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

	// String zeichnen
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

	// Der Optionsstapel gehoert der Schrift und nicht dem Text, <h> gilt aber
	// nur innerhalb dieses einen Strings - renderText() legt je String eine
	// Anzeigeliste an, ueber die Grenze hinweg koennte es also gar nicht
	// wirken. Der Zaehler haelt beides auseinander: ein fehlendes </h> faerbte
	// sonst allen weiteren Text im Spiel kursiv, ein ueberzaehliges nimmt dem
	// Aufrufer seine eigene Sicherung herunter oder greift ins Leere.
	size_t openTags = 0;

	for(size_t i = 0; i < text.length(); i++)
	{
		uint r = static_cast<uint>(text.length() - i - 1);

		unsigned char c = text[i];
		if(c == '\n' || static_cast<char>(c) == '\xB6')
		{
			// Zeilenumbruch
			cursor.x = 0;
			cursor.y += static_cast<int>(options.lineSpacing * lineHeight);
		}
		else if(c == '\t')
		{
			cursor.x += options.tabSize;
			cursor.x /= options.tabSize;
			cursor.x *= options.tabSize;
		}
		else if(r >= 3 && text[i] == '<' && text[i + 1] == 'h' && text[i + 2] == '>')
		{
			optionsStack.push(options);
			options.italic = 4;
			openTags++;
			i += 2;
		}
		else if(r >= 4 && text[i] == '<' && text[i + 1] == '/' && text[i + 2] == 'h' && text[i + 3] == '>')
		{
			if(openTags > 0)
			{
				options = optionsStack.top();
				optionsStack.pop();
				openTags--;
			}
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

	// Was der Text offen gelassen hat, schliesst er hier.
	while(openTags > 0)
	{
		options = optionsStack.top();
		optionsStack.pop();
		openTags--;
	}

	glEnd();
	p_texture->unbind();
}

namespace
{
	// Laenge des Auszeichnungselements, das an dieser Stelle beginnt; 0, wenn
	// dort keines beginnt. Die Schrift kennt genau eines: <h>...</h>.
	size_t tagLength(const std::string& text, size_t position)
	{
		if(text.compare(position, 3, "<h>") == 0) return 3;
		if(text.compare(position, 4, "</h>") == 0) return 4;
		return 0;
	}

	// Der Anfang des Textes bis zum Byte n, dahinter die drei Punkte. Ein
	// angeschnittenes Element faellt ganz weg, ein offen gebliebenes <h> wird
	// geschlossen: <h> legt etwas auf einen Stapel, den erst </h> wieder
	// abtraegt, und dieser Stapel gehoert der Schrift und nicht dem Text - ein
	// abgeschnittenes <h> faerbte allen weiteren Text im Spiel kursiv.
	std::string cutWithEllipsis(const std::string& text, size_t n)
	{
		size_t open = 0, i = 0;
		while(i < n)
		{
			const size_t length = tagLength(text, i);
			if(length == 0) { i++; continue; }
			if(i + length > n) break;
			if(length == 3) open++;
			else if(open > 0) open--;
			i += length;
		}

		std::string out = text.substr(0, i) + "...";
		for(size_t k = 0; k < open; k++) out += "</h>";
		return out;
	}
}

std::string Font::fitText(const std::string& text,
						  int maxWidth)
{
	Vec2i dim;
	measureText(text, &dim, 0);
	if(dim.x <= maxWidth) return text;

	// Binaere Suche ueber die Laenge, gemessen wird jedesmal der ganze Kandidat
	// samt Punkten. Aus den Zeichenpositionen von measureText() liesse sich die
	// Stelle zwar in einem Durchgang ablesen, aber nicht die Breite: die ist
	// nicht cursor.x, sondern das Maximum ueber cursor.x + Zeichenbreite +
	// Kursivneigung, ein Zeilenumbruch setzt cursor.x ohnehin zurueck, und wie
	// breit die drei Punkte werden, haengt davon ab, ob an der Schnittstelle
	// ein <h> offen ist. Den ganzen Kandidaten zu messen fragt genau das, was
	// die Antwort sein soll.
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

	// Wie in renderTextPure(): <h> endet spaetestens mit diesem Text.
	size_t openTags = 0;

	for(size_t i = 0; i < text.length(); i++)
	{
		uint r = static_cast<uint>(text.length() - i - 1);

		// Eine Position je Byte, nicht je Durchlauf. Die Eingabefelder schlagen
		// hier mit dem Byteindex nach, unter dem auch ihr Cursor steht, und ein
		// <h> ist drei Bytes lang bei einem einzigen Durchlauf: die Schleife
		// fuellt deshalb erst auf, was der vorige uebersprungen hat. Der Cursor
		// steht dabei noch genauso wie vor dem Element, das keine Breite hat -
		// wer mitten im <h> steht, steht eben an dessen Stelle.
		if(p_outCharPositions)
			while(p_outCharPositions->size() <= i) p_outCharPositions->push_back(cursor + offset);

		unsigned char c = text[i];
		if(c == '\n' || static_cast<char>(c) == '\xB6')
		{
			// Zeilenumbruch
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
		else if(r >= 3 && text[i] == '<' && text[i + 1] == 'h' && text[i + 2] == '>')
		{
			optionsStack.push(options);
			options.italic = 4;
			openTags++;
			i += 2;
		}
		else if(r >= 4 && text[i] == '<' && text[i + 1] == '/' && text[i + 2] == 'h' && text[i + 3] == '>')
		{
			if(openTags > 0)
			{
				options = optionsStack.top();
				optionsStack.pop();
				openTags--;
			}
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

	// Was der Text offen gelassen hat, schliesst er hier.
	while(openTags > 0)
	{
		options = optionsStack.top();
		optionsStack.pop();
		openTags--;
	}

	// Bis einschliesslich text.length(), denn hinter dem letzten Zeichen darf
	// der Cursor auch stehen. Der Vektor hat damit immer genau ein Feld mehr,
	// als der Text Bytes hat.
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
		unsigned char c = text[i];
		out.append(1, c);

		if(c == '\n' || static_cast<char>(c) == '\xB6')
		{
			// Zeilenumbruch
			cursorX = 0;
		}
		else
		{
			const CharacterInfo& info = charInfo[c];
			int currentWidth = cursorX + info.size.x;

			if(currentWidth > maxWidth)
			{
				// das letzte Leerzeichen in dieser Zeile durch einen Zeilenumbruch ersetzen
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
						back += charInfo[d].size.x + options.charSpacing;
					}
				}

				if(j == out.rend())
				{
					// brutaler Zeilenumbruch
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
		// Der Cache ist jetzt ungueltig!
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