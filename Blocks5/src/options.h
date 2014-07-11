#ifndef _OPTIONS_H
#define _OPTIONS_H

#include "gui_element.h"

class Options : public GUI_Element, public sigslot::has_slots<>
{
public:
	Options(GUI_Element* p_parent);
	~Options();

	void show(GUI_Element* p_focusWhenClosed = 0);
	void handleClick(GUI_Element* p_element);

private:
	GUI_Element* p_focusWhenClosed;
	bool changed;
};

#endif