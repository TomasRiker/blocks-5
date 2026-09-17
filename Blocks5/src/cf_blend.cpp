#include "pch.h"
#include "cf_blend.h"

CF_Blend::CF_Blend()
{
}

CF_Blend::~CF_Blend()
{
}

void CF_Blend::render(double t,
					  uint oldImageID,
					  uint newImageID)
{
	// the old image, fading out over the new one already on the screen
	drawImage(oldImageID, Vec4f(1.0f, 1.0f, 1.0f, static_cast<float>(1.0 - t)));
}
