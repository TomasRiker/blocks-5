#include "pch.h"
#include "cf_star.h"

// The star has 5 points, an outer radius of 1.0 and an inner radius of 0.4.
// The contour is concave, and WebGL has neither the GLU tessellator nor
// display lists. Because the centre lies in the core of the polygon (inner
// radius > 0), a triangle fan from the centre covers the star exactly: 10
// triangles, no tessellator, no list.

CF_Star::CF_Star()
{
}

CF_Star::~CF_Star()
{
}

void CF_Star::renderStar()
{
	const int n = 5;
	const double outerRadius = 1.0;
	const double innerRadius = 0.4;
	const double angleStep = 6.283185307179586476925286766559 / (2 * n);

	Vec2d v[2 * n];
	double angle = 0.0;
	for(int i = 0; i < 2 * n; i++)
	{
		double radius = (i % 2) ? innerRadius : outerRadius;
		v[i] = Vec2d(sin(angle) * radius, -cos(angle) * radius);
		angle += angleStep;
	}

	glBegin(GL_TRIANGLES);
	for(int i = 0; i < 2 * n; i++)
	{
		glVertex2d(0.0, 0.0);
		glVertex2dv(v[i]);
		glVertex2dv(v[(i + 1) % (2 * n)]);
	}
	glEnd();
}

void CF_Star::render(double t,
					 uint oldImageID,
					 uint newImageID)
{
	setupTexCoords();

	// clear the stencil buffer
	glClear(GL_STENCIL_BUFFER_BIT);

	// draw the old image
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

	// draw the star (border)
	glPushMatrix();
	glTranslated(screenSize.x / 2, screenSize.y / 2, 0.0);
	double size = t * t * 2 * screenSize.x;
	glScaled(size, size, 1.0);
	glRotated(t * 180.0, 0.0, 0.0, 1.0);
	glColor4d(0.0, 0.0, 0.0, 1.0);
	renderStar();

	// draw the star into the stencil buffer
	glEnable(GL_STENCIL_TEST);
	glStencilFunc(GL_ALWAYS, 1, ~0);
	glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
	glScaled(0.9, 0.9, 1.0);
	glColor4d(0.0, 0.0, 0.0, 1.0);
	renderStar();

	glPopMatrix();

	// draw the new image into the masked area
	glStencilFunc(GL_EQUAL, 1, ~0);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, newImageID);
	glBegin(GL_QUADS);
	glColor4d(t, t * t, t * t * t, 1.0);
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
	glDisable(GL_STENCIL_TEST);
}