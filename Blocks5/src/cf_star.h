#ifndef _CF_STAR_H
#define _CF_STAR_H

#include "crossfade.h"

/*** Sternblende ***/

class CF_Star : public Crossfade
{
public:
	CF_Star();
	~CF_Star();

	void render(double t, uint oldImageID, uint newImageID);

private:
	GLUtesselator* p_tess;
	uint starList;
};

#endif