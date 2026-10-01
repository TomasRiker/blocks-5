#ifndef _CF_REWIND_H
#define _CF_REWIND_H

#include "crossfade.h"

/*** Crossfade: the tape is rewound ***/

class Texture;

// Restarting a level looks like a video recorder rewinding; cf_rewind.cpp says
// above render() why that hides the cut.
class CF_Rewind : public Crossfade
{
public:
	// The CRT settings' rewind slider, 0..1, as the share of the full-length
	// effect a rewind takes: 0 for none at all, and above it from half the
	// full length up to all of it.
	static float lengthFor(float slider);
	// The crossfade's duration in seconds at a length lengthFor() gave.
	static float durationFor(float length);

	explicit CF_Rewind(float length);
	~CF_Rewind();

	void render(float t, uint oldImageID, uint newImageID);

private:
	// One strip of picture, right across the screen: row y on the screen shows
	// row sourceY of the source image, slipped sideways by shift pixels.
	void drawStrip(uint imageID, int y, int height, int sourceY, float shift) const;

	// One strip of snow. Every call rolls for a fresh spot in the noise image.
	void drawSnow(int y, int height, float alpha) const;

	uint noiseID;

	// The recorder's on-screen display: "REWIND" on the left, two triangles
	// on the right.
	Texture* p_osd;
};

#endif
