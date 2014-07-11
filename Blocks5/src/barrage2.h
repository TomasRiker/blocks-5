#ifndef _BARRAGE2_H
#define _BARRAGE2_H

#include "object.h"

/*** Klasse für eine Blockade ***/

class Barrage2 : public Object
{
public:
	Barrage2(Level& level, const Vec2i& position, bool up, uint color);
	~Barrage2();

	void onRender(int layer, const Vec4d& color);
	void onUpdate();
	bool changeInEditor(int mod);
	void saveAttributes(TiXmlElement* p_target);

	int change(bool up);
	uint getColor() const;

private:
	void updateProperties();

	bool up;
	int shownState;
	uint color;
};

#endif