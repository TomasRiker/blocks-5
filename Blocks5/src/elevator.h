#ifndef _ELEVATOR_H
#define _ELEVATOR_H

#include "object.h"

/*** Klasse für einen Aufzug ***/

class SoundInstance;

class Elevator : public Object
{
public:
	Elevator(Level& level, const Vec2i& position, int dir);
	~Elevator();

	void onRemove();
	void onRender(int layer, const Vec4d& color);
	void onUpdate();
	void onElectricitySwitch(bool on);
	bool changeInEditor(int mod);
	void saveAttributes(TiXmlElement* p_target);
	void saveExtendedAttributes(TiXmlElement* p_target);
	void loadExtendedAttributes(TiXmlElement* p_element);

private:
	int dir;
	int moveCounter;
	int newDir;
	int origDir;
	bool blink;

	static uint numInstances;
	static SoundInstance* p_soundInst;
	static bool soundChanged;
};

#endif