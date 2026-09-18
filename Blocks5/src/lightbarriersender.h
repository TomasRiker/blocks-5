#ifndef _LIGHTBARRIERSENDER_H
#define _LIGHTBARRIERSENDER_H

#include "object.h"

/*** Class for the sender of a light barrier ***/

class LightBarrierSender : public Object
{
public:
	LightBarrierSender(Level& level, const Vec2i& position, int dir);
	~LightBarrierSender();

	void onRender(RenderLayer layer, const Vec4f& color);
	void updateSprites();
	void onUpdate();
	bool changeInEditor(int mod);
	void saveAttributes(TiXmlElement* p_target);

private:
	int dir;
	int counter;
	std::list<Vec2f> beam;
	// The corners of the beam, for the renderer's polyline.
	std::vector<Vec2f> beamPoints;
};

#endif