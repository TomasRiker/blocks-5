#ifndef _LIGHTPANEL_H
#define _LIGHTPANEL_H

#include "panel.h"

/*** Klasse für Lichtschalter-Bodenplatte ***/

class LightPanel : public Panel
{
public:
	LightPanel(Level& level, const Vec2i& position, int subType);
	~LightPanel();

	void onRender(int layer, const Vec4d& color);
	bool changeInEditor(int mod);
	void saveAttributes(TiXmlElement* p_target);

	void onTriggered(Object* p_sender);

private:
	int subType;
};

#endif