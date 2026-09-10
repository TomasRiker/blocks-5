#ifndef _GUI_CHECKBOX_H
#define _GUI_CHECKBOX_H

/*** Class for a check box ***/

#include "gui_element.h"

class GUI_CheckBox : public GUI_Element
{
public:
	DECL_CTOR(GUI_CheckBox);
	~GUI_CheckBox();

	void onRender();
	void onMouseDown(const Vec2i& position, int buttons);
	void onMouseUp(const Vec2i& position, int buttons);
	void onMouseEnter(int buttons);
	// The caption beside the box counts as part of the hit area.
	bool containsPoint(const Vec2i& position);
	void onMouseLeave(int buttons);
	INLINE_GETTYPE("GUI_CheckBox");

	void readAttributes(TiXmlElement* p_element);

	INLINE_GETTER(std::string, getTitle, title);
	INLINE_SETTER(std::string, setTitle, title);
	INLINE_GETTER(bool, isChecked, checked);
	// check() is the user's click: it fires the changed signal. setChecked()
	// is the display catching up and does not - refreshing a display is
	// not an input.
	//
	// Only checked, never newChecked: newChecked is the click in flight. It
	// is written in onMouseDown and read in onMouseUp, and a frame passes in
	// between. Clobbering it here swallows the click that is pressed and not
	// yet released - in the level editor that looks like the electricity
	// switching itself straight back off.
	void check(bool check);
	void setChecked(bool check) { checked = check; }

	INLINE_CONNECTOR(connectChanged, changed);

private:
	std::string title;
	bool checked;
	bool newChecked;
	bool pushed;
	bool mouseOver;

	sigslot::signal1<GUI_Element*> changed;
};

#endif