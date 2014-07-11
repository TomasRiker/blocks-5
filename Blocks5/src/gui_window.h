#ifndef _GUI_WINDOW_H
#define _GUI_WINDOW_H

/*** Klasse für ein Fenster ***/

#include "gui_element.h"

class GUI_Window : public GUI_Element
{
public:
	DECL_CTOR(GUI_Window);
	~GUI_Window();

	void onRender();
	void onRenderEnd();
	void onMouseDown(const Vec2i& position, int buttons);
	void onMouseUp(const Vec2i& position, int buttons);
	void onMouseLeave(int buttons);
	void onMouseMove(const Vec2i& position, const Vec2i& movement, int buttons);
	INLINE_GETTYPE("GUI_Window");

	void readAttributes(TiXmlElement* p_element);

	INLINE_GETTER(std::string, getTitle, title);
	INLINE_SETTER(std::string, setTitle, title);

private:
	std::string title;
	bool moving;
	GUI_Element* p_oldFocusElement;
};

#endif