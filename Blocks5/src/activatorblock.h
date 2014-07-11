#ifndef _ACTIVATORBLOCK_H
#define _ACTIVATORBLOCK_H

#include "object.h"

/*** Klasse für einen Aktivator-Block ***/

class ActivatorBlock : public Object
{
public:
	ActivatorBlock(Level& level, const Vec2i& position, bool shielded);
	~ActivatorBlock();

	void onRender(int layer, const Vec4d& color);
	void onUpdate();
	void onCollision(Object* p_obj);
	void saveAttributes(TiXmlElement* p_target);
	std::string getToolTip() const;

private:
	bool shielded;
	int anim;
};

#endif