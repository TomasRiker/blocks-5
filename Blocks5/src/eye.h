#ifndef _EYE_H
#define _EYE_H

#include "object.h"

/*** Class for eyes (in walls) that turn into enemies ***/

class Eye : public Object
{
public:
	Eye(Level& level, const Vec2i& position, int dir);
	~Eye();

	void onRender(int layer, const Vec4d& color);
	void updateSprites();
	void onUpdate();
	bool changeInEditor(int mod);
	void saveAttributes(TiXmlElement* p_target);

private:
	int dir;
	Vec2d viewDir;
	int closed;
};

#endif