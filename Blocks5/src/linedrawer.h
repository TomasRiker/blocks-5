#ifndef _LINEDRAWER_H
#define _LINEDRAWER_H

/*** Zeichnen von verbundenen Linien, um ATIs langsamen Linienzeichnungsalgorithmus zu umgehen ***/

class LineDrawer
{
public:
	LineDrawer();
	LineDrawer(const std::vector<Vec2f>& points, float width, const Vec4f& color);
	~LineDrawer();

	void draw();
	void setPoints(const std::vector<Vec2f>& points);
	void setWidth(float width);
	void setColor(const Vec4f& color);

	void clear();
	void addPoint(const Vec2f& point);

private:
	void update();

	std::vector<Vec2f> points;
	std::vector<Vec2f> vertices;
	float width;
	Vec4f color;
	bool dirty;
};

#endif