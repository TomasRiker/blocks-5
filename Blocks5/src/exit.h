#ifndef _EXIT_H
#define _EXIT_H

#include "object.h"

/*** Klasse für den Ausgang ***/

class Exit : public Object
{
public:
	Exit(Level& level, const Vec2i& position);
	~Exit();

	void onRemove();
	void onRender(int layer, const Vec4d& color);
	void onUpdate();
};

#endif