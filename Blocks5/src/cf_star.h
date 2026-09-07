#ifndef _CF_STAR_H
#define _CF_STAR_H

#include "crossfade.h"

/*** Star wipe ***/

class CF_Star : public Crossfade
{
public:
	CF_Star();
	~CF_Star();

	void render(double t, uint oldImageID, uint newImageID);

private:
	void renderStar();
};

#endif