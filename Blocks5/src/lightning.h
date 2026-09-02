#ifndef _LIGHTNING_H
#define _LIGHTNING_H

/*** Klasse fuer Blitze ***/

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

	// Verbindungszustand zwischen zwei aufeinanderfolgenden Liniensegmenten eines
	// Astes. Frueher waren das Statics in drawLine(), die sich alle Zweige und
	// beide Durchgaenge geteilt haben.
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