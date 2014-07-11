#ifndef _CF_SLICES_H
#define _CF_SLICES_H

#include "crossfade.h"

/*** Überblendung, bei der das Bild in mehreren Scheiben gedreht wird ***/

class CF_Slices : public Crossfade
{
public:
	CF_Slices();
	~CF_Slices();

	void render(double t, uint oldImageID, uint newImageID);
};

#endif