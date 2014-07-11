#include "pch.h"
#include "cf_slices.h"

CF_Slices::CF_Slices()
{
}

CF_Slices::~CF_Slices()
{
}

void CF_Slices::render(double t,
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
	gluLookAt(0.0, 0.0, -1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);

	glClearColor(0.0, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);

	glEnable(GL_TEXTURE_2D);
	glEnable(GL_CULL_FACE);

	const int n = 20;
	double x = 1.0 - 1.0 / n;
	int tex = screenSize.x / n;
	for(int i = 0; i < n; i++, x -= 2.0 / n)
	{
		double angle = clamp((180.0 + (n - 1) * 10.0) * t - i * 10.0, 0.0, 180.0);
		glPushMatrix();
		glTranslated(x, 0.0, 0.0);
		glRotated(angle, 0.0, 1.0, 0.0);
		glScaled(1.0 / n, 1.0, 1.0);

		// vordere Seite zeichnen
		glBindTexture(GL_TEXTURE_2D, oldImageID);
		glBegin(GL_QUADS);
		double c = 1.0 - angle / 180.0;
		glColor4d(c, c, c, 1.0);
		glTexCoord2i(i * tex + tex, 0);
		glVertex3i(-1, 1, 0);
		glTexCoord2i(i * tex, 0);
		glVertex3i(1, 1, 0);
		glTexCoord2i(i * tex, screenSize.y);
		glVertex3i(1, -1, 0);
		glTexCoord2i(i * tex + tex, screenSize.y);
		glVertex3i(-1, -1, 0);
		glEnd();

		// hintere Seite zeichnen
		glRotated(180.0, 0.0, 1.0, 0.0);
		glBindTexture(GL_TEXTURE_2D, newImageID);
		glBegin(GL_QUADS);
		c = angle / 180.0;
		glColor4d(c, c, c, 1.0);
		glTexCoord2i(i * tex + tex, 0);
		glVertex3i(-1, 1, 0);
		glTexCoord2i(i * tex, 0);
		glVertex3i(1, 1, 0);
		glTexCoord2i(i * tex, screenSize.y);
		glVertex3i(1, -1, 0);
		glTexCoord2i(i * tex + tex, screenSize.y);
		glVertex3i(-1, -1, 0);
		glEnd();

		glPopMatrix();
	}

	glDisable(GL_TEXTURE_2D);
	glDisable(GL_CULL_FACE);

	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
}