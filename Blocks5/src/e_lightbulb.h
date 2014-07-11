#ifndef _E_LIGHTBULB_H
#define _E_LIGHTBULB_H

#include "electronics.h"

/*** Klasse für eine Glühbirne ***/

class E_LightBulb : public Electronics
{
public:
	E_LightBulb(Level& level, const Vec2i& position, int dir);
	~E_LightBulb();

	void onRender(int layer, const Vec4d& color);
	bool changeInEditor(int mod);
	void doLogic();

private:
	bool on;
};

#endif