#include "pch.h"
#include "lightning.h"
#include "texture.h"
#include "engine.h"

Lightning::Lightning()
{
	// Display-Listen generieren
	listBase = glGenLists(2);

	p_lineTexture = Manager<Texture>::inst().request("lightning.png");
}

Lightning::~Lightning()
{
	// Display-Listen löschen
	glDeleteLists(listBase, 2);

	p_lineTexture->release();
}

void Lightning::generate()
{
	alpha = random(1.0, 3.5);
	branches.clear();

	// den Hauptast generieren
	Branch mb;
	mb.thickness = 4.0;
	Vec2d pos = Vec2d(random(50.0, 590.0), random(-200.0, -50.0));
	Vec2d dir(0.0, 1.0);
	int length = random(15, 25);
	for(int i = 0; i < length; i++)
	{
		mb.points.push_back(pos);
		pos += random(20.0, 30.0) * dir;
		dir += Vec2d(random(-0.3, 0.3), random(-0.1, 0.3));
		dir.normalize();
	}

	branches.push_back(mb);

	// weitere Äste generieren
	int details = Engine::inst().getDetails();
	int n = random(4, 10 + details);
	for(int i = 0; i < n; i++)
	{
		Branch sb = generateSecondaryBranch(mb, 5, 8 + details);
		branches.push_back(sb);

		int m = random(4, 6 + details);
		for(int j = 0; j < m; j++)
		{
			Branch tb = generateSecondaryBranch(sb, 4, 6 + details);
			branches.push_back(tb);

			int o = random(2, 2 + details);
			for(int k = 0; k < o; k++)
			{
				Branch qb = generateSecondaryBranch(tb, 2, 2 + details);
				branches.push_back(qb);
			}
		}
	}

	// vorrendern
	glNewList(listBase, GL_COMPILE);
	renderPass(0);
	glEndList();
	glNewList(listBase + 1, GL_COMPILE);
	renderPass(1);
	glEndList();
}

void Lightning::render()
{
	if(alpha < 1.0 / 256.0) return;

	// Pass 0
	glColor4d(0.4, 0.2, 1.0, 0.2 * alpha);
	glCallList(listBase);

	// Pass 1
	glColor4d(1.0, 1.0, 0.75, 0.85 * alpha);
	glCallList(listBase + 1);
}

void Lightning::update()
{
	alpha *= 0.85;
}

void Lightning::renderPass(int pass)
{
	p_lineTexture->bind();

	for(uint i = 0; i < branches.size(); i++)
	{
		const Branch& branch = branches[i];

		double width;
		if(pass == 0) width = branch.thickness * 7.5;
		else width = branch.thickness * 1.5;
		width = clamp(width, 1.0, 20.0);

		glBegin(GL_QUADS);

		for(uint j = 0; j < branch.points.size() - 1; j++)
		{
			drawLine(branch.points[j], branch.points[j + 1], width);
		}

		glEnd();

		if(i == 0)
		{
			p_lineTexture->unbind();
			const Vec2d& last = branches[0].points.back();
			glPointSize(static_cast<float>(width));
			glBegin(GL_POINTS);
			glVertex2dv(last);
			glEnd();
			p_lineTexture->bind();
		}
	}

	p_lineTexture->unbind();
}

Lightning::Branch Lightning::generateSecondaryBranch(const Branch& b,
													 int minLength,
													 int maxLength)
{
	Branch r;
	r.thickness = 0.25 * b.thickness;

	// zwei aufeinanderfolgende Punkte suchen
	int start = random(0, static_cast<int>(b.points.size()) - 2);
	Vec2i pos = b.points[start];
	Vec2d dir = (b.points[start + 1] - b.points[start]).normalize();
	int length = random(minLength, maxLength);
	for(int i = 0; i < length; i++)
	{
		r.points.push_back(pos);
		pos += random(10.0, 15.0) * dir;
		dir += Vec2d(random(-0.4, 0.4), random(-0.4, 0.4));
		dir.normalize();
	}

	return r;
}

void Lightning::drawLine(Vec2d p1,
						 Vec2d p2,
						 double width)
{
	static Vec2d lastEndPoint(-1000.0, -1000.0), lastCorner1, lastCorner2;

	width = clamp(width + 0.5, 1.0, 20.0);
	int w = static_cast<int>(width);

	const int tbl[] = {2, 6, 11, 17, 24, 32, 41, 51, 62, 74, 87, 101, 116, 132, 149, 167, 186, 206, 227};

	Vec2d halfAxis = (p2 - p1).normalize() * 0.5;

	halfAxis = Vec2d(halfAxis.y * -width, halfAxis.x * width);

	int u = tbl[w - 1];

	if(lastEndPoint == p1)
	{
		glTexCoord2i(u, 0);
		glVertex2dv(lastCorner2);
		glTexCoord2i(u + w + 2, 0);
		glVertex2dv(lastCorner1);
	}
	else
	{
		glTexCoord2i(u, 0);
		glVertex2dv(p1 - halfAxis);
		glTexCoord2i(u + w + 2, 0);
		glVertex2dv(p1 + halfAxis);
	}

	lastEndPoint = p2;
	lastCorner1 = p2 + halfAxis;
	lastCorner2 = p2 - halfAxis;

	glTexCoord2i(u + w + 2, 16);
	glVertex2dv(lastCorner1);
	glTexCoord2i(u, 16);
	glVertex2dv(lastCorner2);
}