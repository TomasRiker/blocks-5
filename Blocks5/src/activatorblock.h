#ifndef _ACTIVATORBLOCK_H
#define _ACTIVATORBLOCK_H

#include "object.h"

/*** Class for an activator block ***/

class ActivatorBlock : public Object
{
public:
	ActivatorBlock(Level& level, const Vec2i& position, bool shielded);
	~ActivatorBlock();

	void onRender(int layer, const Vec4d& color);
	void updateSprites();
	void saveAttributes(TiXmlElement* p_target);
	std::string getToolTip() const;

private:
	bool shielded;
};

#endif