#ifndef _CF_COLORBLEND_H
#define _CF_COLORBLEND_H

#include "crossfade.h"

/*** Überblendung: Altes Bild -> Farbe -> Neues Bild ***/

class CF_ColorBlend : public Crossfade
{
public:
	CF_ColorBlend(const Vec3d& color, double timing = 0.5);
	~CF_ColorBlend();

	void render(double t, uint oldImageID, uint newImageID);

private:
	const Vec3d color;
	const double timing;
};

#endif