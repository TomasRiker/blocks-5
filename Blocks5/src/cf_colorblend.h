#ifndef _CF_COLORBLEND_H
#define _CF_COLORBLEND_H

#include "crossfade.h"

/*** Crossfade: old image -> colour -> new image ***/

class CF_ColorBlend : public Crossfade
{
public:
	CF_ColorBlend(const Vec3f& color, float timing = 0.5f);
	~CF_ColorBlend();

	void render(float t, uint oldImageID, uint newImageID);

private:
	const Vec3f color;
	const float timing;
};

#endif