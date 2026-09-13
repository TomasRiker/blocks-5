#ifndef _E_VALUESWITCH_H
#define _E_VALUESWITCH_H

#include "electronics.h"

/*** Class for a switchable 1 or 0 value ***/

class E_ValueSwitch : public Electronics
{
public:
	E_ValueSwitch(Level& level, const Vec2i& position, int value, int dir);
	~E_ValueSwitch();

	void onRender(RenderLayer layer, const Vec4d& color);
	void updateSprites();
	void saveAttributes(TiXmlElement* p_target);
	bool changeInEditor(int mod);
	void onTouchedByPlayer(Player* p_player);
	void onCollision(Object* p_obj);
	void doLogic();

protected:
	int value;
};

#endif