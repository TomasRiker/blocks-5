#include "pch.h"
#include "cf_blend.h"

CF_Blend::CF_Blend()
{
}

CF_Blend::~CF_Blend()
{
}

void CF_Blend::render(double t,
					  uint oldImageID,
					  uint newImageID)
{
	setupTexCoords();

	glEnable(GL_TEXTURE_2D);

	// altes Bild zeichnen
	glBindTexture(GL_TEXTURE_2D, oldImageID);
	glBegin(GL_QUADS);
	glColor4d(1.0, 1.0, 1.0, 1.0 - t);
	glTexCoord2i(0, 0);
	glVertex2i(0, 0);
	glTexCoord2i(screenSize.x, 0);
	glVertex2i(screenSize.x, 0);
	glTexCoord2i(screenSize.x, screenSize.y);
	glVertex2i(screenSize.x, screenSize.y);
	glTexCoord2i(0, screenSize.y);
	glVertex2i(0, screenSize.y);
	glEnd();

	glDisable(GL_TEXTURE_2D);
}