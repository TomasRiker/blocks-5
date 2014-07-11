#ifndef _LIGHTSWITCH_H
#define _LIGHTSWITCH_H

#include "object.h"

/*** Klasse für Lichtschalter ***/

class LightSwitch : public Object
{
public:
	LightSwitch(Level& level, const Vec2i& position);
	~LightSwitch();

	void onRender(int layer, const Vec4d& color);
	void onUpdate();
	void onTouchedByPlayer(Player* p_player);
	void onCollision(Object* p_obj);
};

#endif