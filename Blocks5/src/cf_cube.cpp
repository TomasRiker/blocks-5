#include "pch.h"
#include "cf_cube.h"

CF_Cube::CF_Cube()
{
}

CF_Cube::~CF_Cube()
{
}

void CF_Cube::render(double t,
					 uint oldImageID,
					 uint newImageID)
{
	setupTexCoords();

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	gluPerspective(90.0, 1.0, 0.1, 100.0);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	gluLookAt(0.0, 0.0, -2.0 - sin(t * 3.1415926535897932384626433832795), 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);
	glRotated(90.0 * t, 0.0, 1.0, 0.0);

	glClearColor(0.0, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);

	glEnable(GL_TEXTURE_2D);
	glEnable(GL_CULL_FACE);

	// vordere Würfelseite zeichnen
	glBindTexture(GL_TEXTURE_2D, oldImageID);
	glBegin(GL_QUADS);
	glColor4d(1.0, 1.0, 1.0, 1.0);
	glTexCoord2i(screenSize.x, 0);
	glVertex3i(-1, 1, -1);
	glTexCoord2i(0, 0);
	glVertex3i(1, 1, -1);
	glTexCoord2i(0, screenSize.y);
	glVertex3i(1, -1, -1);
	glTexCoord2i(screenSize.x, screenSize.y);
	glVertex3i(-1, -1, -1);
	glEnd();

	// linke Würfelseite zeichnen
	glRotated(-90.0, 0.0, 1.0, 0.0);
	glBindTexture(GL_TEXTURE_2D, newImageID);
	glBegin(GL_QUADS);
	glColor4d(1.0, 1.0, 1.0, 1.0);
	glTexCoord2i(screenSize.x, 0);
	glVertex3i(-1, 1, -1);
	glTexCoord2i(0, 0);
	glVertex3i(1, 1, -1);
	glTexCoord2i(0, screenSize.y);
	glVertex3i(1, -1, -1);
	glTexCoord2i(screenSize.x, screenSize.y);
	glVertex3i(-1, -1, -1);
	glEnd();

	glDisable(GL_TEXTURE_2D);
	glDisable(GL_CULL_FACE);

	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
}