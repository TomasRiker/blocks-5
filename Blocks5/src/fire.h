#ifndef _FIRE_H
#define _FIRE_H

#include "object.h"

/*** Klasse für Feuer ***/

class Fire : public Object
{
public:
	Fire(Level& level, const Vec2i& position);
	~Fire();

	void onRender(int layer, const Vec4d& color);
	void onUpdate();

private:
	int anim;
};

#endif