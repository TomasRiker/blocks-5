#ifndef _DIAMONDMACHINE_H
#define _DIAMONDMACHINE_H

#include "object.h"

/*** Klasse fuer Diamantenmaschinen ***/

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
	// Der Funkenflug eines Taktes. Der Block kommt frisch aus
	// getFrontObjectAt() und wird nur hier und jetzt angefasst - p_objOnMe
	// bleibt ein Zeiger, der ueber Takte hinweg nur verglichen wird.
	void spawnSparks(Object* p_block);

	// Die Umwandlung ist geplatzt. Laesst die eigenen Funken rueckwaerts
	// laufen, statt sie verschwinden zu lassen.
	void abortConversion();

	// Der Block, der auf der Maschine stand - 0, wenn er zerstoert wurde oder
	// gerade wegteleportiert. Siehe abortConversion().
	Object* findLivingBlock();

	Object* p_objOnMe;

	// Die Kennung, mit der die Einwaertsfunken dieser Umwandlung gezeichnet
	// sind; 0, wenn gerade keine laeuft. Ueber sie findet abortConversion() sie
	// im Partikelsystem wieder, in dem alles andere im Spiel 0 traegt.
	uint sparkId;
	int counter;
	SoundInstance* p_soundInst;
};

#endif