#ifndef _CF_MOSAIC_H
#define _CF_MOSAIC_H

#include "crossfade.h"

/*** Überblendung: Mosaik ***/

class CF_Mosaic : public Crossfade
{
public:
	CF_Mosaic();
	~CF_Mosaic();

	void render(double t, uint oldImageID, uint newImageID);

private:
	uint bufferID;
};

#endif