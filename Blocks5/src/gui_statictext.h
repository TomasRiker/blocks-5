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
	// Ein Text kann auf ein anderes Element zeigen (<For>Name</For>). Dann
	// gehen Mausereignisse dorthin weiter, und ein Klick auf die Beschriftung
	// schaltet die zugehoerige Checkbox oder den Radioknopf - so wie <label
	// for="..."> es im Browser tut. Ein Text ohne w/h wird nie getroffen; wer
	// das benutzt, gibt ihm eine Groesse.
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