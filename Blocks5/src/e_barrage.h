#ifndef _E_BARRIER_H
#define _E_BARRIER_H

#include "electronics.h"

/*** Class for an electronically controlled barrage ***/

class E_Barrage : public Electronics
{
public:
	E_Barrage(Level& level, const Vec2i& position, int dir);
	~E_Barrage();

	void onRender(RenderLayer layer, const Vec4d& color);
	void updateSprites();
	void onUpdate();
	void saveExtendedAttributes(TiXmlElement* p_target);
	void loadExtendedAttributes(TiXmlElement* p_element);
	bool changeInEditor(int mod);
	void doLogic();

private:
	void updateProperties();

	bool up;
	int shownState;
};

#endif