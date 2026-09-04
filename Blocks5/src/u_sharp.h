#ifndef _U_SHARP_H
#define _U_SHARP_H

#include "upscaler.h"

/*** "Scharf" - jeder Quellpixel gleich gross ***/

// Braucht keinen Shader und keinen Bildpuffer; das ist der Rueckfall, wenn von
// den anderen nichts geht.
class U_Sharp : public Upscaler
{
public:
	U_Sharp();
	~U_Sharp();

	const char* getName() const { return "Sharp"; }
	GLint getTextureFilter() const { return GL_NEAREST; }

	// Der eine Grund, warum dieser Filter mehr ist als eine Texturstellung:
	// bei einem krummen Vergroesserungsfaktor verdoppelt Nearest manche
	// Quellpixel und andere nicht - ungleiche Strichstaerken, fransige Schrift.
	// Also nur ganze Stufen, und der Rest bleibt schwarzer Rand.
	bool wantsIntegerScale() const { return true; }
};

#endif
