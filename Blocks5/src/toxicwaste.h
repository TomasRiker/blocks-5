#ifndef _TOXICWASTE_H
#define _TOXICWASTE_H

#include "object.h"

/*** Klasse für ein Giftmüllfass ***/

class ToxicWaste : public Object
{
public:
	ToxicWaste(Level& level, const Vec2i& position);
	~ToxicWaste();

	void onRender(int layer, const Vec4d& color);
	void onUpdate();
	void onExplosion();
	bool reflectLaser(Vec2i& dir, bool lightBarrier);
	bool reflectProjectile(Vec2d& velocity);
	void onFire();
};

#endif