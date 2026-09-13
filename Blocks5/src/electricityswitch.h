#ifndef _ELECTRICITYSWITCH_H
#define _ELECTRICITYSWITCH_H

#include "object.h"

/*** Class for electricity switches ***/

class ElectricitySwitch : public Object
{
public:
	ElectricitySwitch(Level& level, const Vec2i& position);
	~ElectricitySwitch();

	void onRender(RenderLayer layer, const Vec4d& color);
	void updateSprites();
	void onUpdate();
	void onTouchedByPlayer(Player* p_player);
	void onCollision(Object* p_obj);
};

#endif