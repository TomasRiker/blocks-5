#ifndef _E_CLOCK_H
#define _E_CLOCK_H

#include "electronics.h"

/*** Class for a clock ***/

class E_Clock : public Electronics
{
public:
	E_Clock(Level& level, const Vec2i& position, int dir);
	~E_Clock();

	void onRender(RenderLayer layer, const Vec4d& color);
	void updateSprites();
	void saveExtendedAttributes(TiXmlElement* p_target);
	void loadExtendedAttributes(TiXmlElement* p_element);
	bool changeInEditor(int mod);
	void doLogic();

protected:
	int value;
};

#endif