#ifndef _PROJECTILE_H
#define _PROJECTILE_H

#include "object.h"

/*** Klasse für Projektile ***/

class Projectile : public Object
{
public:
	Projectile(Level& level, const Vec2d& positionInPixels, const Vec2d& velocity);
	~Projectile();

	void onRender(int layer, const Vec4d& color);
	void onUpdate();
	void saveExtendedAttributes(TiXmlElement* p_target);
	void loadExtendedAttributes(TiXmlElement* p_element);

private:
	Vec2d positionInPixels;
	Vec2d velocity;
	double speed;
	double distance;
	double life;
	int reflectionCounter;
};

#endif