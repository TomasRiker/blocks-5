#ifndef _BARRAGE2PANEL_H
#define _BARRAGE2PANEL_H

#include "panel.h"

/*** Klasse für eine Blockadenbodenplatte ***/

class Barrage2Panel : public Panel
{
public:
	Barrage2Panel(Level& level, const Vec2i& position, int subType, uint color);
	~Barrage2Panel();

	void onRender(int layer, const Vec4d& color);
	bool changeInEditor(int mod);
	void saveAttributes(TiXmlElement* p_target);

	void onTriggered(Object* p_sender);

private:
	int subType;
	uint color;
};

#endif