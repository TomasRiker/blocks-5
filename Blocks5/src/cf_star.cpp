#include "pch.h"
#include "cf_star.h"

// The star has 5 points, an outer radius of 1.0 and an inner radius of 0.4.
// The contour is concave, and WebGL has neither the GLU tessellator nor
// display lists. Because the centre lies in the core of the polygon (inner
// radius > 0), a triangle fan from the centre covers the star exactly: 10
// triangles, no tessellator, no list.

CF_Star::CF_Star()
{
}

CF_Star::~CF_Star()
{
}

void CF_Star::renderStar(const Vec4f& color)
{
	const int n = 5;
	const double outerRadius = 1.0;
	const double innerRadius = 0.4;
	const double angleStep = 6.283185307179586476925286766559 / (2 * n);

	Vec2d v[2 * n];
	double angle = 0.0;
	for(int i = 0; i < 2 * n; i++)
	{
		double radius = (i % 2) ? innerRadius : outerRadius;
		v[i] = Vec2d(sin(angle) * radius, -cos(angle) * radius);
		angle += angleStep;
	}

	Vec2f positions[2 * n * 3];
	Vec4f colors[2 * n * 3];
	for(int i = 0; i < 2 * n; i++)
	{
		positions[i * 3] = Vec2f(0.0f, 0.0f);
		positions[i * 3 + 1] = static_cast<Vec2f>(v[i]);
		positions[i * 3 + 2] = static_cast<Vec2f>(v[(i + 1) % (2 * n)]);
		colors[i * 3] = colors[i * 3 + 1] = colors[i * 3 + 2] = color;
	}
	Renderer::inst().triangles(positions, colors, 2 * n * 3);
}

void CF_Star::render(double t,
					 uint oldImageID,
					 uint newImageID)
{
	Renderer& renderer = Renderer::inst();
	renderer.setBlend(BM_NORMAL);

	// clear the stencil buffer
	renderer.clearStencil();

	// draw the old image
	drawImage(oldImageID, Vec4f(1.0f, 1.0f, 1.0f, 1.0f));

	// draw the star (border)
	const Vec4f black(0.0f, 0.0f, 0.0f, 1.0f);
	renderer.push();
	renderer.translate(screenSize.x / 2, screenSize.y / 2);
	double size = t * t * 2 * screenSize.x;
	renderer.scale(size, size);
	renderer.rotate(t * 180.0);
	renderStar(black);

	{
		// draw the star into the stencil buffer, in colour too: the ring
		// between the two is the border
		Renderer::StencilWriteScope write(1);
		renderer.scale(0.9, 0.9);
		renderStar(black);
	}

	renderer.pop();

	// draw the new image into the masked area
	{
		Renderer::StencilTestScope test(1);
		drawImage(newImageID, Vec4f(static_cast<float>(t), static_cast<float>(t * t), static_cast<float>(t * t * t), 1.0f));
	}
}
