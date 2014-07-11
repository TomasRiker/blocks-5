#ifndef _TOXICGAS_H
#define _TOXICGAS_H

#include "object.h"

/*** Klasse für Giftgas ***/

class SoundInstance;

class ToxicGas : public Object
{
public:
	ToxicGas(Level& level, const Vec2i& position);
	~ToxicGas();

	void onRemove();
	void onUpdate();
	void frameBegin();

private:
	int spreadCounter;

	static uint numInstances;
	static SoundInstance* p_soundInst;

	void updateSound();
};

#endif