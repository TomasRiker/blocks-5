#include "pch.h"
#include "lightning.h"
#include "texture.h"
#include "engine.h"

Lightning::Lightning()
{
	// There is no lightning bolt yet. Without this, update() would scale an
	// uninitialised value until the first generate().
	alpha = 0.0f;

	// Until the first generate() there is nothing to draw, and drawPass() is
	// asked for a size before it is asked for any geometry.
	for(int pass = 0; pass < 2; pass++) passes[pass].pointSize = 0.0f;

	p_lineTexture = Manager<Texture>::inst().request("lightning.png");
}

Lightning::~Lightning()
{
	p_lineTexture->release();
}

void Lightning::generate()
{
	alpha = random(1.0f, 3.5f);
	branches.clear();

	// generate the main branch
	Branch mb;
	mb.thickness = 4.0f;
	Vec2f pos = Vec2f(random(50.0f, 590.0f), random(-200.0f, -50.0f));
	Vec2f dir(0.0f, 1.0f);
	int length = random(15, 25);
	for(int i = 0; i < length; i++)
	{
		mb.points.push_back(pos);
		pos += random(20.0f, 30.0f) * dir;
		dir += Vec2f(random(-0.3f, 0.3f), random(-0.1f, 0.3f));
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
	if(alpha < 1.0f / 256.0f) return;

	drawPass(0, Vec4f(0.4f, 0.2f, 1.0f, static_cast<float>(0.2f * alpha)));
	drawPass(1, Vec4f(1.0f, 1.0f, 0.75f, static_cast<float>(0.85f * alpha)));
}

void Lightning::update()
{
	alpha *= 0.85f;
}

void Lightning::buildPass(int pass)
{
	Pass& p = passes[pass];
	p.mainBranch.clear();
	p.otherBranches.clear();
	p.pointSize = 0.0f;
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

void Lightning::drawPass(int pass, const Vec4f& color)
{
	const Pass& p = passes[pass];
	if(p.mainBranch.empty()) return;

	Renderer& renderer = Renderer::inst();
	renderer.setTexture(p_lineTexture->ref());
	renderer.quads(renderer.state(), &p.mainBranch[0], static_cast<uint>(p.mainBranch.size()), color);

	// The end point of the main branch, as a single point of the same width.
	// It goes between the two batches and not after them, because the other
	// branches are drawn over it.
	renderer.point(static_cast<Vec2f>(p.endPoint), static_cast<float>(p.pointSize), color);

	if(!p.otherBranches.empty())
	{
		renderer.quads(renderer.state(), &p.otherBranches[0], static_cast<uint>(p.otherBranches.size()), color);
	}
}

float Lightning::branchWidth(const Branch& branch,
							  int pass) const
{
	float width;
	if(pass == 0) width = branch.thickness * 7.5f;
	else width = branch.thickness * 1.5f;
	// The texture only has stripes for the widths 1 to 19.
	return clamp(width, 1.0f, 19.0f);
}

void Lightning::buildBranch(const Branch& branch,
							float width,
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
	r.thickness = 0.25f * b.thickness;

	// find two consecutive points
	int start = random(0, static_cast<int>(b.points.size()) - 2);
	Vec2f pos = b.points[start];
	Vec2f dir = (b.points[start + 1] - b.points[start]).normalize();
	int length = random(minLength, maxLength);
	for(int i = 0; i < length; i++)
	{
		r.points.push_back(pos);
		pos += random(10.0f, 15.0f) * dir;
		dir += Vec2f(random(-0.4f, 0.4f), random(-0.4f, 0.4f));
		dir.normalize();
	}

	return r;
}

void Lightning::addLine(Vec2f p1,
						Vec2f p2,
						float width,
						LineJoint& joint,
						std::vector<QuadVertex>& out)
{
	// tbl has 19 entries (widths 1 to 19). The texture is 256 pixels wide and
	// has no room for a width of 20: the clamp is to 19 and never to 20,
	// since tbl[19] would be one past the end.
	width = clamp(width + 0.5f, 1.0f, 19.0f);
	int w = static_cast<int>(width);

	const int tbl[] = {2, 6, 11, 17, 24, 32, 41, 51, 62, 74, 87, 101, 116, 132, 149, 167, 186, 206, 227};

	Vec2f halfAxis = (p2 - p1).normalize() * 0.5f;

	halfAxis = Vec2f(halfAxis.y * -width, halfAxis.x * width);

	int u = tbl[w - 1];

	// The first two corners join onto the previous segment where there is one,
	// so that consecutive segments of a branch share an edge and the seam does
	// not show.
	const Vec2f start1 = (joint.valid && joint.lastEndPoint == p1) ? joint.lastCorner2 : p1 - halfAxis;
	const Vec2f start2 = (joint.valid && joint.lastEndPoint == p1) ? joint.lastCorner1 : p1 + halfAxis;

	joint.valid = true;
	joint.lastEndPoint = p2;
	joint.lastCorner1 = p2 + halfAxis;
	joint.lastCorner2 = p2 - halfAxis;

	out.push_back(QuadVertex(start1.x, start1.y, u, 0));
	out.push_back(QuadVertex(start2.x, start2.y, u + w + 2, 0));
	out.push_back(QuadVertex(joint.lastCorner1.x, joint.lastCorner1.y, u + w + 2, 16));
	out.push_back(QuadVertex(joint.lastCorner2.x, joint.lastCorner2.y, u, 16));
}