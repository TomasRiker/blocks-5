#ifndef _GUI_EDITBOX_H
#define _GUI_EDITBOX_H

/*** Klasse für ein Eingabefeld ***/

#include "gui_element.h"

class GUI_Button;

class GUI_EditBox : public GUI_Element
{
public:
	DECL_CTOR(GUI_EditBox);
	~GUI_EditBox();

	void onRender();
	void onMouseDown(const Vec2i& position, int buttons);
	void onMouseMove(const Vec2i& position, const Vec2i& movement, int buttons);
	void onKeyEvent(const SDL_KeyboardEvent& event);
	void onTabbedIn();
	INLINE_GETTYPE("GUI_EditBox");

	void readAttributes(TiXmlElement* p_element);

	INLINE_GETTER(std::string, getText, text);
	void setText(const std::string& text);
	INLINE_GETTER(uint, getCursor, cursor);
	INLINE_PGETTER(GUI_Button*, getSubmitButton, p_submitButton);
	INLINE_PSETTER(GUI_Button*, setSubmitButton, p_submitButton);

	INLINE_CONNECTOR(connectChanged, changed);

private:
	void replaceSelection(const std::string& replacement);
	void del();
	void backspace();
	void setCursor(uint cursor, bool shift);
	uint getIndexAt(const Vec2i& position);

	std::string text;
	uint cursor;
	uint selStart;
	uint selEnd;
	int scroll;
	GUI_Button* p_submitButton;

	sigslot::signal1<GUI_Element*> changed;
};

#endif