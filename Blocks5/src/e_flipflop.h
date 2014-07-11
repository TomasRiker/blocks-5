#ifndef _E_FLIPFLOP_H
#define _E_FLIPFLOP_H

#include "electronics.h"

/*** Klasse für Flipflops ***/

class E_FlipFlop : public Electronics
{
public:
	E_FlipFlop(Level& level, const Vec2i& position, int subType, int value, int dir);
	~E_FlipFlop();

	void onRender(int layer, const Vec4d& color);
	void saveAttributes(TiXmlElement* p_target);
	std::string getToolTip() const;
	bool changeInEditor(int mod);
	void doLogic();

protected:
	int subType;
	int value;
};

#endif