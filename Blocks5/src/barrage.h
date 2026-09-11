#ifndef _BARRAGE_H
#define _BARRAGE_H

#include "object.h"

/*** Class for a barrage ***/

class Barrage : public Object
{
public:
	Barrage(Level& level, const Vec2i& position, bool up, uint color);
	~Barrage();

	void onRender(RenderLayer layer, const Vec4d& color);
	void updateSprites();
	void onUpdate();
	bool changeInEditor(int mod);
	void saveAttributes(TiXmlElement* p_target);

	bool change();
	uint getColor() const;

private:
	void updateProperties();

	bool up;
	int shownState;
	uint color;
};

#endif