#ifndef _BOMB_H
#define _BOMB_H

#include "object.h"

/*** Klasse für eine Bombe ***/

class Bomb : public Object
{
public:
	Bomb(Level& level, const Vec2i& position);
	~Bomb();

	void onRender(int layer, const Vec4d& color);
	void onUpdate();
	void onCollect(Player* p_player);
	void onExplosion();
	bool reflectLaser(Vec2i& dir, bool lightBarrier);
	bool reflectProjectile(Vec2d& velocity);
	void onFire();

private:
	int countDown;
};

#endif