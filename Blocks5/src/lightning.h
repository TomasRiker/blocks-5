#ifndef _LIGHTNING_H
#define _LIGHTNING_H

/*** Class for lightning bolts ***/

// For QuadVertex, which a built pass is made of.
#include "renderer.h"

class Texture;

class Lightning
{
public:
	Lightning();
	~Lightning();

	void generate();
	void render();
	void update();

private:
	struct Branch
	{
		float thickness;
		std::vector<Vec2f> points;
	};

	// Joint state between two consecutive line segments of a branch. Every
	// branch and every pass needs its own.
	struct LineJoint
	{
		LineJoint() : valid(false) {}
		bool valid;
		Vec2f lastEndPoint, lastCorner1, lastCorner2;
	};

	// One pass of the bolt as built geometry. Both are built when the bolt is
	// generated and drawn unchanged while it fades; only colour and alpha
	// move. The main branch is kept apart because the point capping it, the
	// renderer's own disc, is drawn between it and the other branches.
	struct Pass
	{
		std::vector<QuadVertex> mainBranch;
		std::vector<QuadVertex> otherBranches;
		Vec2f endPoint;
		float pointSize;
	};

	float alpha;
	std::vector<Branch> branches;
	Pass passes[2];

	void buildPass(int pass);
	void drawPass(int pass, const Vec4f& color);
	float branchWidth(const Branch& branch, int pass) const;
	void buildBranch(const Branch& branch, float width, std::vector<QuadVertex>& out);
	Branch generateSecondaryBranch(const Branch& b, int minLength, int maxLength);
	void addLine(Vec2f p1, Vec2f p2, float width, LineJoint& joint, std::vector<QuadVertex>& out);

	Texture* p_lineTexture;
};

#endif