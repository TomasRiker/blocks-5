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
	// for="Name" is available on any element (see GUI_Element). A text brings
	// only what nobody else needs: w or h at -1 means "as large as the text
	// that is drawn". That is the right thing to give a label - a hand-written
	// width would be a guess and wrong in the other language. Without w/h (0)
	// the text is never hit, and that stays the default.
	bool containsPoint(const Vec2i& position);
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

private:
	std::string text;
	Vec4d color;
	bool wordWrap;
	bool centerText;
};

#endif