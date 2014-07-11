#ifndef _TELEPORTER_H
#define _TELEPORTER_H

#include "object.h"

/*** Klasse für Teleporter ***/

class Teleporter : public Object
{
public:
	Teleporter(Level& level, const Vec2i& position, const Vec2i& targetPosition, int subType);
	~Teleporter();

	void onRender(int layer, const Vec4d& color);
	void onUpdate();
	void saveAttributes(TiXmlElement* p_target);
	std::string getToolTip() const;

	const Vec2i& getTargetPosition() const;
	void setTargetPosition(const Vec2i& targetPosition);

private:
	std::vector<Object*> objectsOnMe;
	Vec2i targetPosition;
	int anim;
	int subType;
};

#endif