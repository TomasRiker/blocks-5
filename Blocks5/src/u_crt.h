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

	bool createGL();
	void destroyGL();
	bool isAvailable() const { return program.isLinked(); }

	void present(const PresentContext& context);

	Vec2d warpToSource(const Vec2d& p) const;
	Vec2d warpToOutput(const Vec2d& s) const;

	// How far the raster stands back from the edge of the glass, in fractions
	// of half the picture width - room for the soft edge and the colour
	// fringes at the edge midpoints too. At curvature 0 it is 0, and the
	// picture then covers exactly what the other filters cover.
	double getOverscan() const;
	bool distortsCursor() const { return curvature > 0.0; }

	void loadConfig(TiXmlElement* p_config);
	void saveConfig(TiXmlElement* p_config);

	// The sliders, 0..1 each; they take effect at once, with no recompile of
	// the shader. 0 means "effect off" on every one of them.
	double getScanline() const { return scanline; }
	double getCurvature() const { return curvature; }
	double getBloom() const { return bloom; }
	double getFlicker() const { return flicker; }
	double getScanFlicker() const { return scanFlicker; }
	double getConvergence() const { return convergence; }
	void setScanline(double value);
	void setCurvature(double value);
	void setBloom(double value);
	void setFlicker(double value);
	void setScanFlicker(double value);
	void setConvergence(double value);

private:
	PresentProgram program;

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

	double scanline;
	double curvature;
	double bloom;
	double flicker;
	double scanFlicker;
	double convergence;
};

#endif
