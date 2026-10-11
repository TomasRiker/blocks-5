#ifndef _BOMB_H
#define _BOMB_H

#include "object.h"

/*** Class for a bomb ***/

class Bomb : public Object
{
public:
	Bomb(Level& level, const Vec2i& position);
	~Bomb();

	void onRender(RenderLayer layer, const Vec4f& color);
	void updateSprites();
	void onUpdate();
	void onCollect(Player* p_player);
	void onExplosion();
	bool reflectLaser(Vec2i& dir, bool lightBarrier);
	bool reflectProjectile(Vec2f& velocity);
	void onFire();
	void saveExtendedAttributes(TiXmlElement* p_target);
	void loadExtendedAttributes(TiXmlElement* p_element);

private:
	int countDown;
};

#endif