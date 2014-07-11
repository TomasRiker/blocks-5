#ifndef _E_CLOCK_H
#define _E_CLOCK_H

#include "electronics.h"

/*** Klasse für einen Taktgeber ***/

class E_Clock : public Electronics
{
public:
	E_Clock(Level& level, const Vec2i& position, int dir);
	~E_Clock();

	void onRender(int layer, const Vec4d& color);
	void saveExtendedAttributes(TiXmlElement* p_target);
	void loadExtendedAttributes(TiXmlElement* p_element);
	bool changeInEditor(int mod);
	void doLogic();

protected:
	int value;
};

#endif