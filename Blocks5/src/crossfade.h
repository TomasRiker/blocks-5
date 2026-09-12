#ifndef _CROSSFADE_H
#define _CROSSFADE_H

// Class for crossfades

class Crossfade
{
public:
	Crossfade();
	virtual ~Crossfade();

	virtual void render(double t, uint oldImageID, uint newImageID);

protected:
	Vec2i screenSize;
	Vec2i screenPow2Size;

	// What the two frame copies are sampled with, so that a crossfade writes
	// its texture coordinates in the game's own pixels. It rides on every
	// GL::bindTexture of an image, the matrix being a function of the binding.
	Vec2d screenTexelScale;
};

#endif