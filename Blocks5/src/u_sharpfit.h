#ifndef _U_SHARPFIT_H
#define _U_SHARPFIT_H

#include "upscaler.h"

/*** "SharpFit" - the nearest look at a fractional scale ***/

// One fetch per output pixel, with a piecewise linear remapping of the texture
// coordinate: constant within a source pixel, a ramp of 1/N source pixels at
// the pixel border. The derivation is above the shader in u_sharpfit.cpp.
//
// The texture MUST be sampled bilinearly - the hardware interpolation *is* the
// filter. With GL_NEAREST nothing but nearest would come out again.
class U_SharpFit : public Upscaler
{
public:
	U_SharpFit();
	~U_SharpFit();

	const char* getName() const { return "SharpFit"; }
	GLint getTextureFilter() const { return GL_LINEAR; }

	bool createGL();
	void destroyGL();
	bool isAvailable() const { return program.isLinked(); }

	void present(const PresentContext& context);

private:
	PresentProgram program;
};

#endif
