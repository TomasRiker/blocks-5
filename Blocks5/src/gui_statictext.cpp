#include "pch.h"
#include "gui_statictext.h"
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
	// Text schreiben
	std::string str = localizeString(text);
	if(wordWrap) str = p_font->adjustText(str, size.x);

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