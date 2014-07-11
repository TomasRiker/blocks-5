#ifndef _E_VALUESWITCH_H
#define _E_VALUESWITCH_H

#include "electronics.h"

/*** Klasse für einen umschaltbaren 1- oder 0-Wert ***/

class E_ValueSwitch : public Electronics
{
public:
	E_ValueSwitch(Level& level, const Vec2i& position, int value, int dir);
	~E_ValueSwitch();

	void onRender(int layer, const Vec4d& color);
	void saveAttributes(TiXmlElement* p_target);
	bool changeInEditor(int mod);
	void onTouchedByPlayer(Player* p_player);
	void onCollision(Object* p_obj);
	void doLogic();

protected:
	int value;
};

#endif