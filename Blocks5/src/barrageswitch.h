#ifndef _BARRAGESWITCH_H
#define _BARRAGESWITCH_H

#include "object.h"

/*** Klasse für Blockadenschalter ***/

class BarrageSwitch : public Object
{
public:
	BarrageSwitch(Level& level, const Vec2i& position, uint color);
	~BarrageSwitch();

	void onRender(int layer, const Vec4d& color);
	void onUpdate();
	void onTouchedByPlayer(Player* p_player);
	void onCollision(Object* p_obj);
	bool changeInEditor(int mod);
	void saveAttributes(TiXmlElement* p_target);

private:
	uint color;
};

#endif