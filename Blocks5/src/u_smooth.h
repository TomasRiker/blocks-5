#ifndef _U_SMOOTH_H
#define _U_SMOOTH_H

#include "upscaler.h"

/*** "Weich" - schlichtes Strecken ***/

// Die Hardware macht die ganze Arbeit, und sie kostet nichts. Verwaschen ist
// sie trotzdem; das ist der Grund, warum es die anderen drei gibt.
class U_Smooth : public Upscaler
{
public:
	U_Smooth();
	~U_Smooth();

	const char* getName() const { return "Smooth"; }
	GLint getTextureFilter() const { return GL_LINEAR; }
};

#endif
