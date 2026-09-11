#ifndef _TOXICWASTE_H
#define _TOXICWASTE_H

#include "object.h"

/*** Class for a toxic waste barrel ***/

class ToxicWaste : public Object
{
public:
	ToxicWaste(Level& level, const Vec2i& position);
	~ToxicWaste();

	void onRender(RenderLayer layer, const Vec4d& color);
	void updateSprites();
	void onUpdate();
	void onExplosion();
	bool reflectLaser(Vec2i& dir, bool lightBarrier);
	bool reflectProjectile(Vec2d& velocity);
	void onFire();
};

#endif