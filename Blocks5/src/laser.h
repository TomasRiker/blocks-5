#ifndef _LASER_H
#define _LASER_H

#include "object.h"
#include "linedrawer.h"

/*** Klasse fuer einen Laser ***/

class SoundInstance;

class Laser : public Object
{
public:
	// zeichnet mit 90.0 * dir
	virtual int getSpriteQuarterTurns() const { return dir; }

	Laser(Level& level, const Vec2i& position, int dir);
	~Laser();

	void onRemove();
	void onRender(int layer, const Vec4d& color);
	void onUpdate();
	void onElectricitySwitch(bool on);
	void frameBegin();
	bool changeInEditor(int mod);
	void saveAttributes(TiXmlElement* p_target);

private:
	int dir;
	int counter;
	double on;
	std::list<Vec2d> beam;
	LineDrawer line;

	static uint numInstances;
	static SoundInstance* p_soundInst;
	static bool soundChanged;
};

#endif