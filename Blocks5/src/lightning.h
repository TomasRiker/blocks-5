#ifndef _LIGHTNING_H
#define _LIGHTNING_H

/*** Class for lightning bolts ***/

// For QuadVertex, which a built pass is made of.
#include "quadarray.h"

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
		double thickness;
		std::vector<Vec2d> points;
	};

	// Joint state between two consecutive line segments of a branch. Every
	// branch and every pass needs its own.
	struct LineJoint
	{
		LineJoint() : valid(false) {}
		bool valid;
		Vec2d lastEndPoint, lastCorner1, lastCorner2;
	};

	// One pass of the bolt as built geometry. Two of them are built when the
	// bolt is generated and then drawn unchanged for the forty or so frames it
	// takes to fade - only the colour and the alpha move. The main branch is
	// kept apart from the rest because the point that caps it is drawn between
	// the two, with the texture off.
	struct Pass
	{
		std::vector<QuadVertex> mainBranch;
		std::vector<QuadVertex> otherBranches;
		Vec2d endPoint;
		double pointSize;
	};

	double alpha;
	std::vector<Branch> branches;
	Pass passes[2];

	void buildPass(int pass);
	void drawPass(int pass);
	double branchWidth(const Branch& branch, int pass) const;
	void buildBranch(const Branch& branch, double width, std::vector<QuadVertex>& out);
	Branch generateSecondaryBranch(const Branch& b, int minLength, int maxLength);
	void addLine(Vec2d p1, Vec2d p2, double width, LineJoint& joint, std::vector<QuadVertex>& out);

	Texture* p_lineTexture;
};

#endif