#include "pch.h"
#include "gui_statictext.h"
#include "gui.h"
#include "filesystem.h"

IMPL_CTOR(GUI_StaticText)
{
	text = "StaticText";
	color = Vec4d(1.0, 1.0, 1.0, 1.0);
	wordWrap = false;
	centerText = false;
}

GUI_StaticText::~GUI_StaticText()
{
}

void GUI_StaticText::onRender()
{
	// write the text
	std::string str = localizeString(text);
	if(wordWrap && size.x > 0) str = p_font->adjustText(str, size.x);

	Vec2i pos(0, 0);
	if(centerText)
	{
		Vec2i dim;
		p_font->measureText(str, &dim, 0);
		pos.x = dim.x / -2;
	}

	p_font->renderText(str, pos, color);
}

void GUI_StaticText::readAttributes(TiXmlElement* p_element)
{
	TiXmlElement* e = p_element->FirstChildElement("Text");
	if(e)
	{
		const char* p_text = e->GetText();
		setText(p_text ? p_text : "");

		const char* p_include = e->Attribute("include");
		if(p_include)
		{
			std::string filename(p_include);
			if(!filename.empty())
			{
				setText(FileSystem::inst().readStringFromFile(filename));
			}
		}
	}

	e = p_element->FirstChildElement("Color");
	if(e)
	{
		e->QueryDoubleAttribute("r", &color.r);
		e->QueryDoubleAttribute("g", &color.g);
		e->QueryDoubleAttribute("b", &color.b);
		e->QueryDoubleAttribute("a", &color.a);
	}

	if(p_element->FirstChildElement("WordWrap")) wordWrap = true;

	if(p_element->FirstChildElement("CenterText")) centerText = true;
}

// w or h set to -1: the hit area is as big as the text that is actually
// drawn. Measured here and not once at load time, because the caption hangs
// off the language - a width remembered at startup would be wrong after a
// switch. That costs one measureText() per frame for the elements the cursor
// is currently over.
bool GUI_StaticText::containsPoint(const Vec2i& position)
{
	if(size.x >= 0 && size.y >= 0) return GUI_Element::containsPoint(position);

	std::string str = localizeString(text);
	if(wordWrap && size.x > 0) str = p_font->adjustText(str, size.x);

	Vec2i dim;
	p_font->measureText(str, &dim, 0);

	const int w = (size.x >= 0) ? size.x : dim.x;
	const int h = (size.y >= 0) ? size.y : dim.y;
	return position.x >= 0 && position.y >= 0 && position.x < w && position.y < h;
}
