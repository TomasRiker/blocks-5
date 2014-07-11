#ifndef _CANNONSWITCH_H
#define _CANNONSWITCH_H

#include "object.h"

/*** Klasse für Kanonenschalter (feuert oder dreht) ***/

class CannonSwitch : public Object
{
public:
	CannonSwitch(Level& level, const Vec2i& position, int subType, uint color);
	~CannonSwitch();

	void onRender(int layer, const Vec4d& color);
	void onUpdate();
	void onTouchedByPlayer(Player* p_player);
	void onCollision(Object* p_obj);
	bool changeInEditor(int mod);
	void saveAttributes(TiXmlElement* p_target);
	std::string getToolTip() const;

private:
	int subType;
	uint color;
};

#endif