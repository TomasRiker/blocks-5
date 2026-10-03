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
	// The CRT settings' rewind slider, 0..1, as how long a rewind takes in
	// seconds: 0 for none at all, 0.5 at the lowest step, 1.25 at the default
	// of 0.5 and 1.65 at the top, in straight lines between.
	static float durationFor(float slider);

	explicit CF_Rewind(float duration);
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
