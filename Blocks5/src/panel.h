#ifndef _PANEL_H
#define _PANEL_H

#include "object.h"

/*** Class for panels ***/

class Panel : public Object
{
public:
	Panel(Level& level, const Vec2i& position);
	virtual ~Panel();

	void onUpdate();

	virtual void onTriggered(Object* p_sender);

private:
	std::vector<Object*> objectsOnMe;
};

#endif