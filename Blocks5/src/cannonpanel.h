#ifndef _CANNONPANEL_H
#define _CANNONPANEL_H

#include "panel.h"

/*** Klasse für Kanonen-Bodenplatte ***/

class CannonPanel : public Panel
{
public:
	CannonPanel(Level& level, const Vec2i& position, uint color);
	~CannonPanel();

	void onRender(int layer, const Vec4d& color);
	bool changeInEditor(int mod);
	void saveAttributes(TiXmlElement* p_target);

	void onTriggered(Object* p_sender);

private:
	uint color;
};

#endif