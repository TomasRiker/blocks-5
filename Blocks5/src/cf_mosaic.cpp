#include "pch.h"
#include "cf_mosaic.h"

CF_Mosaic::CF_Mosaic()
{
	glGenTextures(1, &bufferID);
	GL::bindTexture(bufferID, screenTexelScale);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, screenPow2Size.x, screenPow2Size.y, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

CF_Mosaic::~CF_Mosaic()
{
	GL::deleteTexture(bufferID);
}

void CF_Mosaic::render(double t,
					   uint oldImageID,
					   uint newImageID)
{
	double x = t - 0.5;
	double s = 0.01 + 3.96 * x * x;
	Vec2i size = s * static_cast<Vec2d>(screenSize);

	// render a shrunk-down version of the image
	GL::setTexturing(true);
	GL::bindTexture(t <= 0.5 ? oldImageID : newImageID, screenTexelScale);
	glBegin(GL_QUADS);
	glColor4d(1.0, 1.0, 1.0, 1.0);
	glTexCoord2i(0, 0);
	glVertex2i(0, 0);
	glTexCoord2i(screenSize.x, 0);
	glVertex2i(size.x, 0);
	glTexCoord2i(screenSize.x, screenSize.y);
	glVertex2i(size.x, size.y);
	glTexCoord2i(0, screenSize.y);
	glVertex2i(0, size.y);
	glEnd();

	// copy into the texture
	GL::bindTexture(bufferID, screenTexelScale);
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, screenPow2Size.y - screenSize.y, 0, 0, screenSize.x, screenSize.y);

	// scale back up to full screen size
	glBegin(GL_QUADS);
	glColor4d(1.0, 1.0, 1.0, 1.0);
	glTexCoord2i(0, 0);
	glVertex2i(0, 0);
	glTexCoord2i(size.x, 0);
	glVertex2i(screenSize.x, 0);
	glTexCoord2i(size.x, size.y);
	glVertex2i(screenSize.x, screenSize.y);
	glTexCoord2i(0, size.y);
	glVertex2i(0, screenSize.y);
	glEnd();

	GL::setTexturing(false);
}