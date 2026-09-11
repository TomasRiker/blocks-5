#include "pch.h"
#include "linedrawer.h"
#include "engine.h"

LineDrawer::LineDrawer()
	: width(1.0f)
	, color(1.0f, 1.0f, 1.0f, 1.0f)
	, dirty(true)
{
}

LineDrawer::LineDrawer(const std::vector<Vec2f>& points,
					   float width,
					   const Vec4f& color)
	: points(points)
	, width(width)
	, color(color)
	, dirty(true)
{
}

LineDrawer::~LineDrawer()
{
}

void LineDrawer::draw()
{
	if(dirty)
	{
		update();
		dirty = false;
	}

	// update() gives up on fewer than two points and leaves whatever was
	// there, and draw() clears dirty either way - so this can be empty, and
	// vertices[0] on an empty vector is undefined however little glDrawArrays
	// would then read.
	if(vertices.empty()) return;

	// The sprite batch is drawn with the state standing at the flush, so this
	// raw array has to put it up first. Here and not at the seven call sites:
	// every one of them is inside an onRender, and each reaches this through a
	// LineDrawer of its own, which no static check can follow.
	Engine::inst().flushSprites();

	glColor4fv(color);
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(2, GL_FLOAT, sizeof(Vec2f), vertices[0].value);
	glDrawArrays(GL_QUADS, 0, vertices.size());
	glDisableClientState(GL_VERTEX_ARRAY);
}

void LineDrawer::setPoints(const std::vector<Vec2f>& points)
{
	this->points = points;
	dirty = true;
}

void LineDrawer::setWidth(float width)
{
	this->width = width;
	dirty = true;
}

void LineDrawer::setColor(const Vec4f& color)
{
	this->color = color;
}

void LineDrawer::clear()
{
	points.clear();
	dirty = true;
}

void LineDrawer::addPoint(const Vec2f& point)
{
	points.push_back(point);
	dirty = true;
}

void LineDrawer::update()
{
	if(points.size() < 2) return;

	vertices.clear();

	Vec2f dir, prevDir, up, prevUp;
	for(std::vector<Vec2f>::const_iterator jt = points.begin(), it = jt++;
		jt != points.end();
		++it, ++jt)
	{
		const Vec2f& ptA = *it;
		const Vec2f& ptB = *jt;

		prevDir = dir;
		dir = (ptB - ptA).normalizedCopy();

		prevUp = up;
		up = Vec2f(dir.y, -dir.x) * 0.5f * width;

		if(it != points.begin())
		{
			// Draw the connecting piece.
			// Is it a left turn or a right turn?
			if((prevDir ^ dir) >= 0.0f)
			{
				const float dot = prevUp ^ dir;
				if(dot > 0.0f)
				{
					// left turn <= 90 degrees
					vertices.push_back(ptA);
					vertices.push_back(ptA);
					vertices.push_back(ptA - up);
					vertices.push_back(ptA - prevUp);
				}
				else if(dot < 0.0f)
				{
					// right turn <= 90 degrees
					vertices.push_back(ptA);
					vertices.push_back(ptA);
					vertices.push_back(ptA + prevUp);
					vertices.push_back(ptA + up);
				}
			}
		}

		vertices.push_back(ptA + up);
		vertices.push_back(ptB + up);
		vertices.push_back(ptB - up);
		vertices.push_back(ptA - up);
	}
}