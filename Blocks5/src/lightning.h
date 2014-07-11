#ifndef _LIGHTNING_H
#define _LIGHTNING_H

/*** Klasse für Blitze ***/

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

	double alpha;
	std::vector<Branch> branches;
	uint listBase;

	void renderPass(int pass);
	Branch generateSecondaryBranch(const Branch& b, int minLength, int maxLength);
	void drawLine(Vec2d p1, Vec2d p2, double width);

	Texture* p_lineTexture;
};

#endif