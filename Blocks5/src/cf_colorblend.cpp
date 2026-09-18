#include "pch.h"
#include "cf_colorblend.h"

CF_ColorBlend::CF_ColorBlend(const Vec3f& color,
							 float timing) : color(color), timing(clamp(timing, 0.01f, 0.99f))
{
	// Clamped: render() divides by timing and by 1 - timing.
}

CF_ColorBlend::~CF_ColorBlend()
{
}

void CF_ColorBlend::render(float t,
						   uint oldImageID,
						   uint newImageID)
{
	// the old image until the colour has covered it
	if(t <= timing) drawImage(oldImageID, Vec4f(1.0f, 1.0f, 1.0f, 1.0f));

	// The colour: transparent at both ends, opaque at `timing`, each side
	// on its own slope. One slope for both is right only at timing = 0.5;
	// the light panel's 0.1 would otherwise start 89% opaque.
	const float alpha = t < timing ? t / timing : (1.0f - t) / (1.0f - timing);
	drawColor(Vec4f(static_cast<float>(color.r), static_cast<float>(color.g),
					static_cast<float>(color.b), static_cast<float>(alpha)));
}
