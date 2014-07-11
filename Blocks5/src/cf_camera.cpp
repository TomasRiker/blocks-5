#include "pch.h"
#include "cf_camera.h"

CF_Camera::CF_Camera()
{
}

CF_Camera::~CF_Camera()
{
}

void CF_Camera::render(double t,
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

	double s1 = sin(t * 1.5707963267948966192313216916398);
	double s2 = sin(t * 3.1415926535897932384626433832795);
	double y = s1 * s1 * s1 * s1 * s1 * s1 * 4.0;
	double l = y + s2 * s2 * s2 * s2 * 4.0;

	gluLookAt(0.0, y, -1.0, 0.0, l, 0.0, 0.0, 1.0, 0.0);

	glClearColor(0.0, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);

	glEnable(GL_TEXTURE_2D);

	// altes Bild zeichnen
	glBindTexture(GL_TEXTURE_2D, oldImageID);
	glBegin(GL_QUADS);
	glColor4d(1.0, 1.0, 1.0, 1.0);
	glTexCoord2i(screenSize.x, 0);
	glVertex3i(-1, 1, 0);
	glTexCoord2i(0, 0);
	glVertex3i(1, 1, 0);
	glTexCoord2i(0, screenSize.y);
	glVertex3i(1, -1, 0);
	glTexCoord2i(screenSize.x, screenSize.y);
	glVertex3i(-1, -1, 0);
	glEnd();

	// neues Bild zeichnen
	glBindTexture(GL_TEXTURE_2D, newImageID);
	glBegin(GL_QUADS);
	glColor4d(1.0, 1.0, 1.0, 1.0);
	glTexCoord2i(screenSize.x, 0);
	glVertex3i(-1, 5, 0);
	glTexCoord2i(0, 0);
	glVertex3i(1, 5, 0);
	glTexCoord2i(0, screenSize.y);
	glVertex3i(1, 3, 0);
	glTexCoord2i(screenSize.x, screenSize.y);
	glVertex3i(-1, 3, 0);
	glEnd();

	glDisable(GL_TEXTURE_2D);

	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
}