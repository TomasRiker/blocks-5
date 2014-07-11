#include "pch.h"
#include "cf_colorblend.h"

CF_ColorBlend::CF_ColorBlend(const Vec3d& color,
							 double timing) : color(color), timing(timing)
{
}

CF_ColorBlend::~CF_ColorBlend()
{
}

void CF_ColorBlend::render(double t,
						   uint oldImageID,
						   uint newImageID)
{
	setupTexCoords();

	if(t <= timing)
	{
		// altes Bild zeichnen
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, oldImageID);
		glBegin(GL_QUADS);
		glColor4d(1.0, 1.0, 1.0, 1.0);
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

	// Farbfläche zeichnen
	glBegin(GL_QUADS);
	double alpha = 1.0 - (1.0 / (1.0 - timing)) * abs(t - timing);
	glColor4d(color.r, color.g, color.b, alpha);
	glVertex2i(0, 0);
	glVertex2i(screenSize.x, 0);
	glVertex2i(screenSize.x, screenSize.y);
	glVertex2i(0, screenSize.y);
	glEnd();
}