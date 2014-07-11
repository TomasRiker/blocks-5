#ifndef _EYE_H
#define _EYE_H

#include "object.h"

/*** Klasse für Augen (in Wänden), die sich in Gegner verwandeln ***/

class Eye : public Object
{
public:
	Eye(Level& level, const Vec2i& position, int dir);
	~Eye();

	void onRender(int layer, const Vec4d& color);
	void onUpdate();
	bool changeInEditor(int mod);
	void saveAttributes(TiXmlElement* p_target);

private:
	int dir;
	Vec2d viewDir;
	int closed;
};

#endif