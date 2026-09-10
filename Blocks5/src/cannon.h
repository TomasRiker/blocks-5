#ifndef _CANNON_H
#define _CANNON_H

#include "object.h"

/*** Class for a cannon ***/

class Cannon : public Object
{
public:
	Cannon(Level& level, const Vec2i& position, uint color, int dir);
	~Cannon();

	void onRender(int layer, const Vec4d& color);
	void updateSprites();
	void onUpdate();
	bool changeInEditor(int mod);
	void saveAttributes(TiXmlElement* p_target);
	void saveExtendedAttributes(TiXmlElement* p_target);
	void loadExtendedAttributes(TiXmlElement* p_element);

	uint getColor() const;
	bool fire();
	void rotate();

private:
	uint color;
	int dir;
	double shownDir;
	int reload;
};

#endif