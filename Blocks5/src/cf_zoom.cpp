#include "pch.h"
#include "cf_zoom.h"

CF_Zoom::CF_Zoom(const Vec2i& targetIn,
				 const Vec2i& targetOut) : targetIn(targetIn), targetOut(targetOut)
{
}

CF_Zoom::~CF_Zoom()
{
}

void CF_Zoom::render(double t,
					 uint oldImageID,
					 uint newImageID)
{
	setupTexCoords();

	glClearColor(0.0, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);

	Vec2d targetPos;

	uint imageID;
	if(t <= 0.5)
	{
		t = 2.0 * t;
		imageID = oldImageID;
		targetPos = Vec2d(-1.0, -1.0) + static_cast<Vec2d>(2 * targetIn) / screenSize;
	}
	else
	{
		t = 2.0 - 2.0 * t;
		imageID = newImageID;
		targetPos = Vec2d(-1.0, -1.0) + static_cast<Vec2d>(2 * targetOut) / screenSize;
	}

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, imageID);

	double ts = t - 25.0 * 0.01;
	for(int i = 0; i < 25; i++)
	{
		t = clamp(ts, 0.0, 1.0);

		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();

		double fov = 90.0 - t * t * 80.0;
		gluPerspective(fov, 1.0, 0.001, 10.0);
		glTranslated(1.0 / screenSize.x, -1.0 / screenSize.y, 0.0);

		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();

		double s;
		if(t > 0.81649) s = 1.0;
		else s = sin(1.5 * t * t * 1.5707963267948966192313216916398);
		Vec2d camPos = s * targetPos;
		double z = -1.0 + 0.99 * t;
		double r = t * t * 1.5;
		gluLookAt(camPos.x, camPos.y, z, camPos.x, camPos.y, 0.0, -sin(r), -cos(r), 0.0);

		// Bild zeichnen
		glBegin(GL_QUADS);
		glColor4d(1.0, 1.0, 1.0, 1.0 - 0.5 * t * t);
		glTexCoord2i(0, 0);
		glVertex3i(-1, -1, 0);
		glTexCoord2i(screenSize.x, 0);
		glVertex3i(1, -1, 0);
		glTexCoord2i(screenSize.x, screenSize.y);
		glVertex3i(1, 1, 0);
		glTexCoord2i(0, screenSize.y);
		glVertex3i(-1, 1, 0);
		glEnd();

		glPopMatrix();
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);

		ts += 0.01;
	}

	glDisable(GL_TEXTURE_2D);

	// Farbfläche zeichnen
	glBegin(GL_QUADS);
	glColor4d(1.0, 1.0, 1.0, t * t);
	glVertex2i(0, 0);
	glVertex2i(screenSize.x, 0);
	glVertex2i(screenSize.x, screenSize.y);
	glVertex2i(0, screenSize.y);
	glEnd();
}