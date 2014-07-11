#ifndef _RAIL_H
#define _RAIL_H

/*** Klasse für eine Schiene ***/

#include "object.h"

class Rail : public Object
{
	friend class Elevator;

public:
	Rail(Level& level, const Vec2i& position, int subType, int dir);
	~Rail();

	void onRender(int layer, const Vec4d& color);
	void onUpdate();
	bool changeInEditor(int mod);
	void saveAttributes(TiXmlElement* p_target);
	std::string getToolTip() const;

private:
	int subType;
	int dir;
};

#endif