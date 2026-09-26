#ifndef _GUI_STATICTEXT_H
#define _GUI_STATICTEXT_H

/*** Class for a static text ***/

#include "gui_element.h"

class GUI_StaticText : public GUI_Element
{
public:
	DECL_CTOR(GUI_StaticText);
	~GUI_StaticText();

	void onRender();
	// for="Name" works on any element that keeps GUI_Element's mouse
	// handlers - a text, an image, a plain element. What a text adds is
	// w or h at -1, "as large as the text that is drawn", which is what a
	// label wants: a hand-written width would be wrong in the other language.
	// Without w/h (0) the text is never hit, and that stays the default.
	bool containsPoint(const Vec2i& position);
	INLINE_GETTYPE("GUI_StaticText");

	// The text as it reaches the screen - localized, its bindings expanded,
	// wrapped to the element's width - and how big that is. One rule for the
	// render, the hit test and the test hook, which reports the size so that
	// a page grown out of its box fails an assertion.
	std::string getDrawnText();
	Vec2i measureDrawnText();

	void readAttributes(TiXmlElement* p_element);

	INLINE_GETTER(std::string, getText, text);
	INLINE_SETTER(std::string, setText, text);
	INLINE_GETTER(Vec4f, getColor, color);
	INLINE_SETTER(Vec4f, setColor, color);
	INLINE_GETTER(bool, getWordWrap, wordWrap);
	INLINE_SETTER(bool, setWordWrap, wordWrap);
	INLINE_GETTER(bool, getCenterText, centerText);
	INLINE_SETTER(bool, setCenterText, centerText);

private:
	std::string text;
	Vec4f color;
	bool wordWrap;
	bool centerText;
};

#endif