#include "pch.h"
#include "lightning.h"
#include "texture.h"
#include "engine.h"

Lightning::Lightning()
{
	// There is no lightning bolt yet. Without this, update() would scale an
	// uninitialised value until the first generate().
	alpha = 0.0;

	// generate the display lists
	listBase = glGenLists(2);

	p_lineTexture = Manager<Texture>::inst().request("lightning.png");
}

Lightning::~Lightning()
{
	// delete the display lists
	glDeleteLists(listBase, 2);

	p_lineTexture->release();
}

void Lightning::generate()
{
	alpha = random(1.0, 3.5);
	branches.clear();

	// generate the main branch
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

	// generate the further branches
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

#ifndef __EMSCRIPTEN__
	// pre-render
	glNewList(listBase, GL_COMPILE);
	renderPass(0);
	glEndList();
	glNewList(listBase + 1, GL_COMPILE);
	renderPass(1);
	glEndList();
#endif
}

void Lightning::render()
{
	if(alpha < 1.0 / 256.0) return;

	// Pass 0
	glColor4d(0.4, 0.2, 1.0, 0.2 * alpha);
#ifdef __EMSCRIPTEN__
	renderPass(0);   // WebGL has no display lists
#else
	glCallList(listBase);
#endif

	// Pass 1
	glColor4d(1.0, 1.0, 0.75, 0.85 * alpha);
#ifdef __EMSCRIPTEN__
	renderPass(1);
#else
	glCallList(listBase + 1);
#endif
}

void Lightning::update()
{
	alpha *= 0.85;
}

void Lightning::renderPass(int pass)
{
	if(branches.empty()) return;

	p_lineTexture->bind();

	// main branch
	const double mainWidth = branchWidth(branches[0], pass);
	glBegin(GL_QUADS);
	renderBranch(branches[0], mainWidth);
	glEnd();

	// end point of the main branch
	p_lineTexture->unbind();
	glPointSize(static_cast<float>(mainWidth));
	glBegin(GL_POINTS);
	glVertex2dv(branches[0].points.back());
	glEnd();
	p_lineTexture->bind();

	// all remaining branches in a single block
	glBegin(GL_QUADS);
	for(uint i = 1; i < branches.size(); i++)
	{
		renderBranch(branches[i], branchWidth(branches[i], pass));
	}
	glEnd();

	p_lineTexture->unbind();
}

double Lightning::branchWidth(const Branch& branch,
							  int pass) const
{
	double width;
	if(pass == 0) width = branch.thickness * 7.5;
	else width = branch.thickness * 1.5;
	// The texture only has stripes for the widths 1 to 19.
	return clamp(width, 1.0, 19.0);
}

void Lightning::renderBranch(const Branch& branch,
							 double width)
{
	LineJoint joint;
	for(uint j = 0; j + 1 < branch.points.size(); j++)
	{
		drawLine(branch.points[j], branch.points[j + 1], width, joint);
	}
}

Lightning::Branch Lightning::generateSecondaryBranch(const Branch& b,
													 int minLength,
													 int maxLength)
{
	Branch r;
	r.thickness = 0.25 * b.thickness;

	// find two consecutive points
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
						 double width,
						 LineJoint& joint)
{
	// tbl has 19 entries (widths 1 to 19). The texture is 256 pixels wide and
	// has no room for a width of 20: the clamp is to 19 and never to 20,
	// since tbl[19] would be one past the end.
	width = clamp(width + 0.5, 1.0, 19.0);
	int w = static_cast<int>(width);

	const int tbl[] = {2, 6, 11, 17, 24, 32, 41, 51, 62, 74, 87, 101, 116, 132, 149, 167, 186, 206, 227};

	Vec2d halfAxis = (p2 - p1).normalize() * 0.5;

	halfAxis = Vec2d(halfAxis.y * -width, halfAxis.x * width);

	int u = tbl[w - 1];

	if(joint.valid && joint.lastEndPoint == p1)
	{
		glTexCoord2i(u, 0);
		glVertex2dv(joint.lastCorner2);
		glTexCoord2i(u + w + 2, 0);
		glVertex2dv(joint.lastCorner1);
	}
	else
	{
		glTexCoord2i(u, 0);
		glVertex2dv(p1 - halfAxis);
		glTexCoord2i(u + w + 2, 0);
		glVertex2dv(p1 + halfAxis);
	}

	joint.valid = true;
	joint.lastEndPoint = p2;
	joint.lastCorner1 = p2 + halfAxis;
	joint.lastCorner2 = p2 - halfAxis;

	glTexCoord2i(u + w + 2, 16);
	glVertex2dv(joint.lastCorner1);
	glTexCoord2i(u, 16);
	glVertex2dv(joint.lastCorner2);
}