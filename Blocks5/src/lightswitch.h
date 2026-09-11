#ifndef _LIGHTSWITCH_H
#define _LIGHTSWITCH_H

#include "object.h"

/*** Class for light switches ***/

class LightSwitch : public Object
{
public:
	LightSwitch(Level& level, const Vec2i& position);
	~LightSwitch();

	void onRender(RenderLayer layer, const Vec4d& color);
	void updateSprites();
	void onUpdate();
	void onTouchedByPlayer(Player* p_player);
	void onCollision(Object* p_obj);
};

#endif