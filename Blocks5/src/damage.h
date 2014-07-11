#ifndef _DAMAGE_H
#define _DAMAGE_H

#include "object.h"

/*** Klasse für verbrannten Boden ***/

class Damage : public Object
{
public:
	Damage(Level& level, const Vec2i& position, double rotation = -1.0);
	~Damage();

	void onRender(int layer, const Vec4d& color);
	void onUpdate();
	void saveAttributes(TiXmlElement* p_target);

private:
	double rotation;
};

#endif