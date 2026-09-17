#ifndef _U_SHARP_H
#define _U_SHARP_H

#include "upscaler.h"

/*** "Sharp" - every source pixel the same size ***/

// Brings no shader: the base class's pass-through program draws it, and the
// whole of the filter is GL_NEAREST plus the integer step below.
class U_Sharp : public Upscaler
{
public:
	U_Sharp();
	~U_Sharp();

	const char* getName() const { return "Sharp"; }
	GLint getTextureFilter() const { return GL_NEAREST; }

	// The one reason this filter is more than a texture setting: at a
	// fractional scale nearest doubles some source pixels and not others -
	// uneven stroke widths, ragged lettering. Hence whole steps only, and the
	// rest stays black surround.
	bool wantsIntegerScale() const { return true; }
};

#endif
