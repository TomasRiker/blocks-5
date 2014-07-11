#ifndef _E_PULSEPANEL_H
#define _E_PULSEPANEL_H

#include "electronics.h"

/*** Klasse für eine Puls-Bodenplatte ***/

class E_PulsePanel : public Electronics
{
public:
	E_PulsePanel(Level& level, const Vec2i& position, int pulseValue, int dir);
	~E_PulsePanel();

	void onRender(int layer, const Vec4d& color);
	void onUpdate();
	void saveAttributes(TiXmlElement* p_target);
	void saveExtendedAttributes(TiXmlElement* p_target);
	void loadExtendedAttributes(TiXmlElement* p_element);
	std::string getToolTip() const;
	bool changeInEditor(int mod);
	void doLogic();

protected:
	int pulseValue;
	int value;
	std::vector<Object*> objectsOnMe;
};

#endif