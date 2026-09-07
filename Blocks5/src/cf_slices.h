#ifndef _CF_SLICES_H
#define _CF_SLICES_H

#include "crossfade.h"

/*** Crossfade that turns the picture in several slices ***/

class CF_Slices : public Crossfade
{
public:
	CF_Slices();
	~CF_Slices();

	void render(double t, uint oldImageID, uint newImageID);
};

#endif