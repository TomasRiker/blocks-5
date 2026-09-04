#ifndef _U_SHARPFIT_H
#define _U_SHARPFIT_H

#include "upscaler.h"

/*** "Scharf, angepasst" - nearest-Optik bei krummem Vergroesserungsfaktor ***/

// Ein Fetch je Ausgabepixel, mit einer stueckweise linearen Umrechnung der
// Texturkoordinate: innerhalb eines Quellpixels konstant, an der Pixelgrenze
// eine Rampe von 1/N Quellpixeln. Die Herleitung steht ueber dem Shader in
// u_sharpfit.cpp.
//
// Die Textur MUSS bilinear abgetastet werden - die Hardware-Interpolation *ist*
// der Filter. Mit GL_NEAREST kaeme wieder nur nearest heraus.
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
