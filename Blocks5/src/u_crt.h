#ifndef _U_CRT_H
#define _U_CRT_H

#include "upscaler.h"

/*** "CRT monitor" - a screen from the nineties ***/

// Everything that gives this filter its character is a const above the shader
// in u_crt.cpp, meant to be edited; the six sliders here are the ones that are
// matters of taste rather than tuning.
//
// The barrel distortion goes through the mouse as well: warpToSource() is the
// shader's formula once more in C++, warpToOutput() its inverse, and
// Engine::getCursorPosition() hangs off it.
class U_Crt : public Upscaler
{
public:
	U_Crt();
	~U_Crt();

	const char* getName() const { return "Crt"; }
	// Sharp-fit remaps the texture coordinate such that the hardware
	// interpolation gives the nearest result; the CRT filter does the same,
	// only softer. Both need GL_LINEAR for it.
	GLint getTextureFilter() const { return GL_LINEAR; }

	// The base class's program plus the locations of the uniforms below.
	bool createGL();

	void present(const PresentContext& context);

	Vec2f warpToSource(const Vec2f& p) const;
	Vec2f warpToOutput(const Vec2f& s) const;

	// How far the raster stands back from the edge of the glass, in fractions
	// of half the picture width - room for the soft edge and the colour
	// fringes at the edge midpoints too. At curvature 0 it is 0, and the
	// picture then covers exactly what the other filters cover.
	float getOverscan() const;
	bool distortsCursor() const { return curvature > 0.0f; }

	void loadConfig(TiXmlElement* p_config);
	void saveConfig(TiXmlElement* p_config);

	// The sliders, 0..1 each; they take effect at once, with no recompile of
	// the shader. 0 means "effect off" on every one of them.
	float getScanline() const { return scanline; }
	float getCurvature() const { return curvature; }
	float getBloom() const { return bloom; }
	float getFlicker() const { return flicker; }
	float getScanFlicker() const { return scanFlicker; }
	float getConvergence() const { return convergence; }
	void setScanline(float value);
	void setCurvature(float value);
	void setBloom(float value);
	void setFlicker(float value);
	void setScanFlicker(float value);
	void setConvergence(float value);

protected:
	const char* getFragmentSource() const;

private:
	// The uniform locations that exist only here. One per line so that
	// Tools/verify.py sees them - it overlooks a collected declaration,
	// and that is exactly what "convergence" depended on.
	int locScanline;
	int locCurvature;
	int locBloom;
	int locFlicker;
	int locScanFlicker;
	int locConvergence;
	int locOverscan;
	int locTime;
	int locScanPhase;

	// The frame size as it last stood in the PresentContext; getOverscan() uses
	// it to convert source rows and columns into fractions of the picture.
	Vec2i frameSize;

	float scanline;
	float curvature;
	float bloom;
	float flicker;
	float scanFlicker;
	float convergence;
};

#endif
