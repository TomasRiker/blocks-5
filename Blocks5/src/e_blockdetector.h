#ifndef _E_BLOCKDETECTOR_H
#define _E_BLOCKDETECTOR_H

#include "electronics.h"

/*** Klasse für einen Block-Detektor ***/

class E_BlockDetector : public Electronics
{
public:
	E_BlockDetector(Level& level, const Vec2i& position, int dir);
	~E_BlockDetector();

	void onRender(int layer, const Vec4d& color);
	bool changeInEditor(int mod);
	void doLogic();
};

#endif