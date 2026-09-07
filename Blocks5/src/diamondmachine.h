#ifndef _DIAMONDMACHINE_H
#define _DIAMONDMACHINE_H

#include "object.h"

/*** Class for diamond machines ***/

class SoundInstance;

class DiamondMachine : public Object
{
public:
	DiamondMachine(Level& level, const Vec2i& position);
	~DiamondMachine();

	void onRender(int layer, const Vec4d& color);
	void updateSprites();
	void onUpdate();

private:
	// The shower of sparks for one tick. The block comes fresh from
	// getFrontObjectAt() and is touched only here and now - p_objOnMe stays a
	// pointer that is only ever compared across ticks.
	void spawnSparks(Object* p_block);

	// The conversion has fallen through. Runs the machine's own sparks
	// backwards instead of letting them vanish.
	void abortConversion();

	// The block that stood on the machine - 0 if it has been destroyed or has
	// just been teleported away. See abortConversion().
	Object* findLivingBlock();

	Object* p_objOnMe;

	// The id the inward sparks of this conversion are marked with; 0 while
	// none is running. It is how abortConversion() finds them again in the
	// particle system, where everything else in the game carries 0.
	uint sparkId;
	int counter;
	SoundInstance* p_soundInst;
};

#endif