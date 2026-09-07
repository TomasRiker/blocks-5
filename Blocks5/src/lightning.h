#ifndef _LIGHTNING_H
#define _LIGHTNING_H

/*** Class for lightning bolts ***/

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

	double alpha;
	std::vector<Branch> branches;
	uint listBase;

	void renderPass(int pass);
	double branchWidth(const Branch& branch, int pass) const;
	void renderBranch(const Branch& branch, double width);
	Branch generateSecondaryBranch(const Branch& b, int minLength, int maxLength);
	void drawLine(Vec2d p1, Vec2d p2, double width, LineJoint& joint);

	Texture* p_lineTexture;
};

#endif