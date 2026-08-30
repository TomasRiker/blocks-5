#ifndef _GUI_STATICTEXT_H
#define _GUI_STATICTEXT_H

/*** Klasse für einen statischen Text ***/

#include "gui_element.h"

class GUI_StaticText : public GUI_Element
{
public:
	DECL_CTOR(GUI_StaticText);
	~GUI_StaticText();

	void onRender();
	// Ein Text kann auf ein anderes Element zeigen (for="Name") - so wie
	// <label for="..."> es im Browser tut. Bei einer Checkbox oder einem
	// Radioknopf schaltet ein Klick auf die Beschriftung das Element um, bei
	// allem anderen - Eingabefeldern vor allem - setzt er den Fokus dorthin.
	// Fuer die Trefferflaeche gilt: w oder h auf -1 heisst "so gross wie der
	// gezeichnete Text". Das ist die richtige Angabe fuer eine Beschriftung -
	// eine von Hand eingetragene Breite waere geraten und in einer anderen
	// Sprache falsch. Ohne w/h (also 0) wird der Text nie getroffen, das war
	// schon immer so und bleibt die Voreinstellung.
	bool containsPoint(const Vec2i& position);
	void onMouseDown(const Vec2i& position, int buttons);
	void onMouseUp(const Vec2i& position, int buttons);
	void onMouseEnter(int buttons);
	void onMouseLeave(int buttons);
	INLINE_GETTYPE("GUI_StaticText");

	void readAttributes(TiXmlElement* p_element);

	INLINE_GETTER(std::string, getText, text);
	INLINE_SETTER(std::string, setText, text);
	INLINE_GETTER(Vec4d, getColor, color);
	INLINE_SETTER(Vec4d, setColor, color);
	INLINE_GETTER(bool, getWordWrap, wordWrap);
	INLINE_SETTER(bool, setWordWrap, wordWrap);
	INLINE_GETTER(bool, getCenterText, centerText);
	INLINE_SETTER(bool, setCenterText, centerText);
	INLINE_GETTER(std::string, getLinkedElement, linkedElement);
	INLINE_SETTER(std::string, setLinkedElement, linkedElement);

private:
	// Das verknuepfte Element, relativ zum eigenen Elternelement gesucht.
	// 0, wenn nichts verknuepft ist oder der Name ins Leere zeigt.
	GUI_Element* getLinkedTarget();

	std::string text;
	std::string linkedElement;
	Vec4d color;
	bool wordWrap;
	bool centerText;
};

#endif