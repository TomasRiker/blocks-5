#ifndef _EXIT_H
#define _EXIT_H

#include "object.h"

/*** Class for the exit ***/

class Exit : public Object
{
public:
	Exit(Level& level, const Vec2i& position);
	~Exit();

	void onRemove();
	void onRender(RenderLayer layer, const Vec4f& color);
	void updateSprites();
	void onUpdate();
};

#endif