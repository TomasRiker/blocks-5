#include "pch.h"
#include "lightning.h"
#include "texture.h"
#include "engine.h"

Lightning::Lightning()
{
	// There is no lightning bolt yet. Without this, update() would scale an
	// uninitialised value until the first generate().
	alpha = 0.0;

	// Until the first generate() there is nothing to draw, and drawPass() is
	// asked for a size before it is asked for any geometry.
	for(int pass = 0; pass < 2; pass++) passes[pass].pointSize = 0.0;

	p_lineTexture = Manager<Texture>::inst().request("lightning.png");
}

Lightning::~Lightning()
{
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

	// The geometry is settled here and does not move again: a bolt is drawn for
	// as long as it takes to fade, and only its colour and alpha change.
	for(int pass = 0; pass < 2; pass++) buildPass(pass);
}

void Lightning::render()
{
	if(alpha < 1.0 / 256.0) return;

	// Pass 0
	glColor4d(0.4, 0.2, 1.0, 0.2 * alpha);
	drawPass(0);

	// Pass 1
	glColor4d(1.0, 1.0, 0.75, 0.85 * alpha);
	drawPass(1);
}

void Lightning::update()
{
	alpha *= 0.85;
}

void Lightning::buildPass(int pass)
{
	Pass& p = passes[pass];
	p.mainBranch.clear();
	p.otherBranches.clear();
	p.pointSize = 0.0;
	if(branches.empty()) return;

	// main branch
	p.pointSize = branchWidth(branches[0], pass);
	p.endPoint = branches[0].points.back();
	buildBranch(branches[0], p.pointSize, p.mainBranch);

	// all remaining branches in a single block
	for(uint i = 1; i < branches.size(); i++)
	{
		buildBranch(branches[i], branchWidth(branches[i], pass), p.otherBranches);
	}
}

void Lightning::drawPass(int pass)
{
	const Pass& p = passes[pass];
	if(p.mainBranch.empty()) return;

	p_lineTexture->bind();
	drawQuadArray(&p.mainBranch[0], static_cast<uint>(p.mainBranch.size()));
	GL::setTexturing(false);

	// The end point of the main branch, as a single point of the same width.
	// It goes between the two batches and not after them, because the other
	// branches are drawn over it.
	glPointSize(static_cast<float>(p.pointSize));
	glBegin(GL_POINTS);
	glVertex2dv(p.endPoint);
	glEnd();

	if(!p.otherBranches.empty())
	{
		p_lineTexture->bind();
		drawQuadArray(&p.otherBranches[0], static_cast<uint>(p.otherBranches.size()));
		GL::setTexturing(false);
	}
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

void Lightning::buildBranch(const Branch& branch,
							double width,
							std::vector<QuadVertex>& out)
{
	LineJoint joint;
	for(uint j = 0; j + 1 < branch.points.size(); j++)
	{
		addLine(branch.points[j], branch.points[j + 1], width, joint, out);
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

void Lightning::addLine(Vec2d p1,
						Vec2d p2,
						double width,
						LineJoint& joint,
						std::vector<QuadVertex>& out)
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

	// The first two corners join onto the previous segment where there is one,
	// so that consecutive segments of a branch share an edge and the seam does
	// not show.
	const Vec2d start1 = (joint.valid && joint.lastEndPoint == p1) ? joint.lastCorner2 : p1 - halfAxis;
	const Vec2d start2 = (joint.valid && joint.lastEndPoint == p1) ? joint.lastCorner1 : p1 + halfAxis;

	joint.valid = true;
	joint.lastEndPoint = p2;
	joint.lastCorner1 = p2 + halfAxis;
	joint.lastCorner2 = p2 - halfAxis;

	out.push_back(QuadVertex(start1.x, start1.y, u, 0));
	out.push_back(QuadVertex(start2.x, start2.y, u + w + 2, 0));
	out.push_back(QuadVertex(joint.lastCorner1.x, joint.lastCorner1.y, u + w + 2, 16));
	out.push_back(QuadVertex(joint.lastCorner2.x, joint.lastCorner2.y, u, 16));
}