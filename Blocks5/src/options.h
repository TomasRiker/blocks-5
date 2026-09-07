#ifndef _OPTIONS_H
#define _OPTIONS_H

#include "gui_element.h"

class Options : public GUI_Element, public sigslot::has_slots<>
{
public:
	Options(GUI_Element* p_parent);
	~Options();

	void show(GUI_Element* p_focusWhenClosed = 0);
	// Escape = Cancel, Return = OK. With the CRT settings window open, both
	// keys close only that one.
	void onKeyEvent(const SDL_KeyboardEvent& event);

	// Picks up the result while a key button is waiting for a key. The key
	// grab lives in the Engine and runs alongside.
	void onUpdate();

	void handleClick(GUI_Element* p_element);

private:
	void applyKeyGrab(int key);

	GUI_Element* p_focusWhenClosed;
	bool changed;

	// Which key button is currently waiting ("PrimaryKey"/"SecondaryKey"),
	// and for which action. Empty means none.
	std::string grabButton;
	std::string grabAction;
};

#endif