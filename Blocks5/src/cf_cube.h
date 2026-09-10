#ifndef _CF_CUBE_H
#define _CF_CUBE_H

#include "crossfade.h"

/*** Mac-style crossfade ***/

class CF_Cube : public Crossfade
{
public:
	CF_Cube();
	~CF_Cube();

	void render(double t, uint oldImageID, uint newImageID);
};

#endif