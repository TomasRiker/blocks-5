#ifndef _E_GATE_H
#define _E_GATE_H

#include "electronics.h"

/*** Klasse für logische Gatter ***/

class E_Gate : public Electronics
{
public:
	E_Gate(Level& level, const Vec2i& position, int subType, int dir);
	~E_Gate();

	void onRender(int layer, const Vec4d& color);
	void saveAttributes(TiXmlElement* p_target);
	std::string getToolTip() const;
	bool changeInEditor(int mod);
	void doLogic();

protected:
	int subType;
};

#endif