#include "pch.h"
#include "cf_star.h"

typedef void (APIENTRY *TESS_CALLBACK_FN)(void);

CF_Star::CF_Star()
{
	p_tess = gluNewTess();
	gluTessCallback(p_tess, GLU_TESS_BEGIN, reinterpret_cast<TESS_CALLBACK_FN>(glBegin));
	gluTessCallback(p_tess, GLU_TESS_END, reinterpret_cast<TESS_CALLBACK_FN>(glEnd));
	gluTessCallback(p_tess, GLU_TESS_VERTEX, reinterpret_cast<TESS_CALLBACK_FN>(glVertex2dv));

	// Stern erzeugen
	starList = glGenLists(1);
	glNewList(starList, GL_COMPILE);
	gluTessBeginPolygon(p_tess, 0);
	gluTessBeginContour(p_tess);

	const int n = 5;
	const double outerRadius = 1.0;
	const double innerRadius = 0.4;
	const double angleStep = 6.283185307179586476925286766559 / (2 * n);
	double xyz[n * 2][3];

	double angle = 0.0;
	for(int i = 0; i < 2 * n; i++)
	{
		double radius = (i % 2) ? innerRadius : outerRadius;
		xyz[i][0] = sin(angle) * radius;
		xyz[i][1] = -cos(angle) * radius;
		xyz[i][2] = 0.0;
		gluTessVertex(p_tess, xyz[i], xyz[i]);
		angle += angleStep;
	}

	gluTessEndContour(p_tess);
	gluTessEndPolygon(p_tess);
	glEndList();
}

CF_Star::~CF_Star()
{
	gluDeleteTess(p_tess);
	glDeleteLists(starList, 1);
}

void CF_Star::render(double t,
					 uint oldImageID,
					 uint newImageID)
{
	setupTexCoords();

	// Stencil-Buffer leeren
	glClear(GL_STENCIL_BUFFER_BIT);

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

	// Stern (Rand) zeichnen
	glPushMatrix();
	glTranslated(screenSize.x / 2, screenSize.y / 2, 0.0);
	double size = t * t * 2 * screenSize.x;
	glScaled(size, size, 1.0);
	glRotated(t * 180.0, 0.0, 0.0, 1.0);
	glColor4d(0.0, 0.0, 0.0, 1.0);
	glCallList(starList);

	// Stern in den Stencil-Buffer zeichnen
	glEnable(GL_STENCIL_TEST);
	glStencilFunc(GL_ALWAYS, 1, ~0);
	glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
	glScaled(0.9, 0.9, 1.0);
	glColor4d(0.0, 0.0, 0.0, 1.0);
	glCallList(starList);

	glPopMatrix();

	// neues Bild in den maskierten Bereich zeichnen
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