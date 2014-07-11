#ifndef _CF_BLEND_H
#define _CF_BLEND_H

#include "crossfade.h"

/*** Normale Überblendung ***/

class CF_Blend : public Crossfade
{
public:
	CF_Blend();
	~CF_Blend();

	void render(double t, uint oldImageID, uint newImageID);
};

#endif