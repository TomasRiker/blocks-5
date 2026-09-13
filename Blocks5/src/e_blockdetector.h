#ifndef _E_BLOCKDETECTOR_H
#define _E_BLOCKDETECTOR_H

#include "electronics.h"

/*** Class for a block detector ***/

class E_BlockDetector : public Electronics
{
public:
	E_BlockDetector(Level& level, const Vec2i& position, int dir);
	~E_BlockDetector();

	void onRender(RenderLayer layer, const Vec4d& color);
	void updateSprites();
	bool changeInEditor(int mod);
	void doLogic();
};

#endif