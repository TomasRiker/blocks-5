#ifndef _U_SMOOTH_H
#define _U_SMOOTH_H

#include "upscaler.h"

/*** "Smooth" - plain stretching ***/

// The hardware does all the work, and it costs nothing. The picture is washed
// out all the same; that is the reason the other three exist.
class U_Smooth : public Upscaler
{
public:
	U_Smooth();
	~U_Smooth();

	const char* getName() const { return "Smooth"; }
	GLint getTextureFilter() const { return GL_LINEAR; }
};

#endif
