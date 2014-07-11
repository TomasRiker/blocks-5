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