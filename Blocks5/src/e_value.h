#ifndef _E_VALUE_H
#define _E_VALUE_H

#include "electronics.h"

/*** Class for a static 1 or 0 value ***/

class E_Value : public Electronics
{
public:
	E_Value(Level& level, const Vec2i& position, int value, int dir);
	~E_Value();

	void onRender(int layer, const Vec4d& color);
	void updateSprites();
	void saveAttributes(TiXmlElement* p_target);
	std::string getToolTip() const;
	bool changeInEditor(int mod);
	void doLogic();

protected:
	int value;
};

#endif