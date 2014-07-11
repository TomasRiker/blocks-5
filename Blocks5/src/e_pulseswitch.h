#ifndef _E_PULSESWITCH_H
#define _E_PULSESWITCH_H

#include "electronics.h"

/*** Klasse für einen Pulsschalter ***/

class E_PulseSwitch : public Electronics
{
public:
	E_PulseSwitch(Level& level, const Vec2i& position, int pulseValue, int dir);
	~E_PulseSwitch();

	void onRender(int layer, const Vec4d& color);
	void saveAttributes(TiXmlElement* p_target);
	void saveExtendedAttributes(TiXmlElement* p_target);
	void loadExtendedAttributes(TiXmlElement* p_element);
	std::string getToolTip() const;
	bool changeInEditor(int mod);
	void onTouchedByPlayer(Player* p_player);
	void onCollision(Object* p_obj);
	void doLogic();

protected:
	int pulseValue;
	int value;
};

#endif