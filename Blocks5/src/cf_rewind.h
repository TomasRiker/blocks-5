#ifndef _CF_REWIND_H
#define _CF_REWIND_H

#include "crossfade.h"

/*** Crossfade: the tape is rewound ***/

class Texture;

// Restarting a level looks like a video recorder in rewind. Why exactly that
// hides the cut is written above render() in cf_rewind.cpp.
class CF_Rewind : public Crossfade
{
public:
	CF_Rewind();
	~CF_Rewind();

	void render(double t, uint oldImageID, uint newImageID);

private:
	// One strip of picture, right across the screen: row y on the screen shows
	// row sourceY of the source image, slipped sideways by shift pixels.
	void drawStrip(int y, int height, int sourceY, double shift) const;

	// One strip of snow. Every call rolls for a fresh spot in the noise image.
	void drawSnow(int y, int height, double alpha) const;

	uint noiseID;

	// The recorder's on-screen display as an image: "REWIND" on the left, the
	// two triangles on the right. And when the effect started, which keeps the
	// arrows blinking in time from the first second rather than in an arbitrary
	// phase.
	Texture* p_osd;
	uint startTicks;
};

#endif
