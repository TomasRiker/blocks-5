#include "pch.h"
#include "cf_colorblend.h"

CF_ColorBlend::CF_ColorBlend(const Vec3d& color,
							 double timing) : color(color), timing(timing)
{
}

CF_ColorBlend::~CF_ColorBlend()
{
}

void CF_ColorBlend::render(double t,
						   uint oldImageID,
						   uint newImageID)
{
	// the old image until the colour has covered it
	if(t <= timing) drawImage(oldImageID, Vec4f(1.0f, 1.0f, 1.0f, 1.0f));

	// the colour, opaque at `timing` and transparent at both ends
	const double alpha = 1.0 - (1.0 / (1.0 - timing)) * abs(t - timing);
	drawColor(Vec4f(static_cast<float>(color.r), static_cast<float>(color.g),
					static_cast<float>(color.b), static_cast<float>(alpha)));
}
