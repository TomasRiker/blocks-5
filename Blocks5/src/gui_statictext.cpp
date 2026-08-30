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
	linkedElement = "";
}

GUI_StaticText::~GUI_StaticText()
{
}

void GUI_StaticText::onRender()
{
	// Text schreiben
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

	e = p_element->FirstChildElement("For");
	if(e)
	{
		const char* p_name = e->GetText();
		setLinkedElement(p_name ? p_name : "");
	}
}

GUI_Element* GUI_StaticText::getLinkedTarget()
{
	if(linkedElement.empty() || !p_parent) return 0;
	return p_parent->getChild(linkedElement);
}

// w oder h auf -1: die Trefferflaeche ist so gross wie der Text, der wirklich
// gezeichnet wird. Gemessen wird hier und nicht einmalig beim Laden, denn die
// Beschriftung haengt an der Sprache - eine beim Start gemerkte Breite waere
// nach dem Umschalten falsch. Das kostet ein measureText() je Bild fuer die
// Elemente, ueber denen der Zeiger gerade steht.
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

// Ein Umschalter und ein Eingabefeld wollen Verschiedenes.
//
// Checkbox und Radioknopf bekommen den ganzen Satz Mausereignisse: sie schalten
// beim Loslassen nur um, wenn sie sich fuer "unter der Maus" halten, und
// nebenbei leuchtet das Ziel auf, solange die Maus ueber der Beschriftung steht
// - genau die richtige Rueckmeldung.
//
// Alles andere - vor allem Eingabefelder - bekommt statt dessen den Fokus. Die
// Mausposition durchzureichen waere dort falsch: sie ist auf die Beschriftung
// bezogen, und ein Eingabefeld setzt daraus die Schreibmarke, die dann
// irgendwo im Text landet.
static bool wantsTheWholeClick(GUI_Element* p_target)
{
	const std::string type = p_target->getType();
	return type == "GUI_CheckBox" || type == "GUI_RadioButton";
}

void GUI_StaticText::onMouseDown(const Vec2i& position,
								 int buttons)
{
	GUI_Element* p_target = getLinkedTarget();
	if(!p_target) return;

	if(wantsTheWholeClick(p_target)) p_target->onMouseDown(position, buttons);
	else if(buttons & 1) p_target->focus();
}

void GUI_StaticText::onMouseUp(const Vec2i& position,
							   int buttons)
{
	GUI_Element* p_target = getLinkedTarget();
	if(p_target && wantsTheWholeClick(p_target)) p_target->onMouseUp(position, buttons);
}

void GUI_StaticText::onMouseEnter(int buttons)
{
	GUI_Element* p_target = getLinkedTarget();
	if(p_target && wantsTheWholeClick(p_target)) p_target->onMouseEnter(buttons);
}

void GUI_StaticText::onMouseLeave(int buttons)
{
	GUI_Element* p_target = getLinkedTarget();
	if(p_target && wantsTheWholeClick(p_target)) p_target->onMouseLeave(buttons);
}
