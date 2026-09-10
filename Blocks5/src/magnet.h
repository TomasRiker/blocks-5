#ifndef _MAGNET_H
#define _MAGNET_H

#include "object.h"

/*** Class for magnets ***/

class Magnet : public Object
{
public:
	Magnet(Level& level, const Vec2i& position);
	~Magnet();

	void onRender(int layer, const Vec4d& color);
	void updateSprites();
	void onUpdate();
	void onTouchedByPlayer(Player* p_player);
	void onCollision(Object* p_obj);
};

#endif