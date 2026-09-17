#ifndef _LASER_H
#define _LASER_H

#include "object.h"

/*** Class for a laser ***/

class SoundInstance;

class Laser : public Object
{
public:
	Laser(Level& level, const Vec2i& position, int dir);
	~Laser();

	void onRemove();
	void onRender(RenderLayer layer, const Vec4d& color);
	void updateSprites();
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
	// The corners of the beam, for the renderer's polyline.
	std::vector<Vec2f> beamPoints;

	static uint numInstances;
	static SoundInstance* p_soundInst;
	static bool soundChanged;
};

#endif