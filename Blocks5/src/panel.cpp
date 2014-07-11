#include "pch.h"
#include "panel.h"

Panel::Panel(Level& level,
			 const Vec2i& position) : Object(level, 500)
{
	warpTo(position);
	flags = OF_FIXED;
}

Panel::~Panel()
{
}

void Panel::onUpdate()
{
	// Befindet sich ein Objekt auf dem Panel, das vorher noch nicht da war?
	std::vector<Object*> newObjectsOnMe = level.getObjectsAt2(position);
	for(std::vector<Object*>::const_iterator i = newObjectsOnMe.begin(); i != newObjectsOnMe.end(); ++i)
	{
		Object* p_obj = *i;
		if(p_obj == this) continue;

		if(std::find(objectsOnMe.begin(), objectsOnMe.end(), p_obj) == objectsOnMe.end())
		{
			if(p_obj->getFlags() & OF_TRIGGER_PANELS)
			{
				// Panel auslösen
				onTriggered(p_obj);
				break;
			}
		}
	}

	objectsOnMe = newObjectsOnMe;
}

void Panel::onTriggered(Object* p_sender)
{
}