#ifndef _PROJECTILE_H
#define _PROJECTILE_H

#include "object.h"

/*** Class for projectiles ***/

class Projectile : public Object
{
public:
	// Pixels per second of a shot as a cannon fires it; a reflection only
	// slows one down.
	static const float CANNON_SPEED;

	Projectile(Level& level, const Vec2f& positionInPixels, const Vec2f& velocity);
	~Projectile();

	void onRender(RenderLayer layer, const Vec4f& color);
	void onUpdate();
	void saveExtendedAttributes(TiXmlElement* p_target);
	void loadExtendedAttributes(TiXmlElement* p_element);

private:
	Vec2f positionInPixels;
	Vec2f velocity;
	float speed;
	float distance;
	float life;
	int reflectionCounter;
};

#endif