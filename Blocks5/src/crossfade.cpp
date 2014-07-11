#include "pch.h"
#include "crossfade.h"
#include "engine.h"

Crossfade::Crossfade()
{
	Engine& engine = Engine::inst();
	screenSize = engine.getScreenSize();
	screenPow2Size = engine.getScreenPow2Size();
}

Crossfade::~Crossfade()
{
}

void Crossfade::render(double t,
					   uint oldImageID,
					   uint newImageID)
{
}

void Crossfade::setupTexCoords()
{
	// Pixel-Texturkoordinaten
	glPushAttrib(GL_TRANSFORM_BIT);
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glScaled(1.0 / screenPow2Size.x, -1.0 / screenPow2Size.y, 1.0);
	glPopAttrib();
}