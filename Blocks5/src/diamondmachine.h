#ifndef _DIAMONDMACHINE_H
#define _DIAMONDMACHINE_H

#include "object.h"

/*** Klasse für Diamantenmaschinen ***/

class SoundInstance;

class DiamondMachine : public Object
{
public:
	DiamondMachine(Level& level, const Vec2i& position);
	~DiamondMachine();

	void onRender(int layer, const Vec4d& color);
	void onUpdate();

private:
	Object* p_objOnMe;
	int counter;
	SoundInstance* p_soundInst;
};

#endif