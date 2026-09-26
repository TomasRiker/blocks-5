#include "pch.h"
#include "cf_star.h"

// The star has 5 points, an outer radius of 1.0 and an inner radius of 0.4.
// The contour is concave, but the centre lies in the core of the polygon
// (inner radius > 0), so 10 triangles from the centre cover it exactly and
// no tessellator is needed. The renderer draws GL_TRIANGLES and no fans, so
// the centre is repeated in every triangle.

CF_Star::CF_Star()
{
}

CF_Star::~CF_Star()
{
}

void CF_Star::renderStar(const Vec4f& color)
{
	const int n = 5;
	const float outerRadius = 1.0f;
	const float innerRadius = 0.4f;
	const float angleStep = 6.283185307179586476925286766559f / (2 * n);

	Vec2f v[2 * n];
	float angle = 0.0f;
	for(int i = 0; i < 2 * n; i++)
	{
		float radius = (i % 2) ? innerRadius : outerRadius;
		v[i] = Vec2f(sinf(angle) * radius, -cosf(angle) * radius);
		angle += angleStep;
	}

	Vec2f positions[2 * n * 3];
	Vec4f colors[2 * n * 3];
	for(int i = 0; i < 2 * n; i++)
	{
		positions[i * 3] = Vec2f(0.0f, 0.0f);
		positions[i * 3 + 1] = v[i];
		positions[i * 3 + 2] = v[(i + 1) % (2 * n)];
		colors[i * 3] = colors[i * 3 + 1] = colors[i * 3 + 2] = color;
	}
	Renderer::inst().triangles(positions, colors, 2 * n * 3);
}

void CF_Star::render(float t,
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
	renderer.translate(static_cast<float>(screenSize.x / 2), static_cast<float>(screenSize.y / 2));
	float size = t * t * 2 * screenSize.x;
	renderer.scale(size, size);
	renderer.rotate(t * 180.0f);
	renderStar(black);

	{
		// draw the star into the stencil buffer, in colour too: the ring
		// between the two is the border
		Renderer::StencilWriteScope write(1);
		renderer.scale(0.9f, 0.9f);
		renderStar(black);
	}

	renderer.pop();

	// draw the new image into the masked area
	{
		Renderer::StencilTestScope test(1);
		drawImage(newImageID, Vec4f(t, t * t, t * t * t, 1.0f));
	}
}
