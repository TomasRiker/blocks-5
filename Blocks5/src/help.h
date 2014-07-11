#ifndef _HELP_H
#define _HELP_H

#include "gui_element.h"

class Help : public GUI_Element, public sigslot::has_slots<>
{
public:
	Help(GUI_Element* p_parent);
	~Help();

	void show(GUI_Element* p_focusWhenClosed = 0);
	void handleClick(GUI_Element* p_element);

private:
	void updatePage();

	GUI_Element* p_focusWhenClosed;
	int page;
};

#endif