#ifndef _MIRROR_H
#define _MIRROR_H

#include "object.h"

/*** Klasse fuer einen Spiegel ***/

class Mirror : public Object
{
public:
	// zeichnet mit 90.0 * dir
	virtual int getSpriteQuarterTurns() const { return dir; }

	Mirror(Level& level, const Vec2i& position, int subType, int dir);
	~Mirror();

	void onRender(int layer, const Vec4d& color);
	void onUpdate();
	bool reflectLaser(Vec2i& dir, bool lightBarrier);
	bool reflectProjectile(Vec2d& velocity);
	bool changeInEditor(int mod);
	void saveAttributes(TiXmlElement* p_target);
	std::string getToolTip() const;

private:
	int subType;
	int dir;
};

#endif