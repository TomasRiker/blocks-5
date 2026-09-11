#ifndef _FIRE_H
#define _FIRE_H

#include "object.h"

/*** Class for fire ***/

class Fire : public Object
{
public:
	Fire(Level& level, const Vec2i& position);
	~Fire();

	void onRender(RenderLayer layer, const Vec4d& color);
	void updateSprites();
	void onUpdate();

private:
	int anim;
};

#endif