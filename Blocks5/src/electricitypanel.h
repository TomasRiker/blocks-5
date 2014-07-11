#ifndef _ELECTRICITYPANEL_H
#define _ELECTRICITYPANEL_H

#include "panel.h"

/*** Klasse für eine Stromschalterbodenplatte ***/

class ElectricityPanel : public Panel
{
public:
	ElectricityPanel(Level& level, const Vec2i& position, int subType);
	~ElectricityPanel();

	void onRender(int layer, const Vec4d& color);
	bool changeInEditor(int mod);
	void saveAttributes(TiXmlElement* p_target);

	void onTriggered(Object* p_sender);

private:
	int subType;
};

#endif