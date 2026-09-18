#ifndef _DAMAGE_H
#define _DAMAGE_H

#include "object.h"

/*** Class for burnt ground ***/

class Damage : public Object
{
public:
	Damage(Level& level, const Vec2i& position, float rotation = -1.0f);
	~Damage();

	void onRender(RenderLayer layer, const Vec4f& color);
	void updateSprites();
	void onUpdate();
	void saveAttributes(TiXmlElement* p_target);

private:
	float rotation;
};

#endif