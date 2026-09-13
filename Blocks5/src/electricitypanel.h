#ifndef _ELECTRICITYPANEL_H
#define _ELECTRICITYPANEL_H

#include "panel.h"

/*** Class for an electricity switch panel ***/

class ElectricityPanel : public Panel
{
public:
	ElectricityPanel(Level& level, const Vec2i& position, int subType);
	~ElectricityPanel();

	void onRender(RenderLayer layer, const Vec4d& color);
	void updateSprites();
	bool changeInEditor(int mod);
	void saveAttributes(TiXmlElement* p_target);

	void onTriggered(Object* p_sender);

private:
	int subType;
};

#endif