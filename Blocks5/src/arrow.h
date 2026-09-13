#ifndef _ARROW_H
#define _ARROW_H

#include "object.h"

/*** Class for a one-way arrow ***/

class Arrow : public Object
{
public:
	Arrow(Level& level, const Vec2i& position, int dir);
	~Arrow();

	void onRender(RenderLayer layer, const Vec4d& color);
	void updateSprites();
	void onUpdate();
	bool allowMovement(const Vec2i& dir);
	bool changeInEditor(int mod);
	void saveAttributes(TiXmlElement* p_target);

	void turn();

private:
	int dir;
	double shownDir;
	double dirVel;
	double shownAlpha;
	int counter;
};

#endif