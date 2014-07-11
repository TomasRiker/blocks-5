#ifndef _LIGHTBARRIERSENDER_H
#define _LIGHTBARRIERSENDER_H

#include "object.h"
#include "linedrawer.h"

/*** Klasse für den Sender einer Lichtschranke ***/

class LightBarrierSender : public Object
{
public:
	LightBarrierSender(Level& level, const Vec2i& position, int dir);
	~LightBarrierSender();

	void onRender(int layer, const Vec4d& color);
	void onUpdate();
	bool changeInEditor(int mod);
	void saveAttributes(TiXmlElement* p_target);

private:
	int dir;
	int counter;
	std::list<Vec2d> beam;
	LineDrawer line;
};

#endif