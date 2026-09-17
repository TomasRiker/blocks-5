#include "pch.h"
#include "cf_slices.h"

CF_Slices::CF_Slices()
{
}

CF_Slices::~CF_Slices()
{
}

void CF_Slices::render(double t,
					   uint oldImageID,
					   uint newImageID)
{
	// Twenty vertical slices, each with the old image on its front and the
	// new one on its back, turning over one after another.
	const Mat4 projection = Mat4::perspective(90.0, 1.0, 0.1, 100.0);
	const Mat4 view = Mat4::lookAt(0.0, 0.0, -1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);

	Renderer::inst().clear(Vec4f(0.0f, 0.0f, 0.0f, 1.0f));

	const Vec3f face[4] = {Vec3f(-1.0f, 1.0f, 0.0f), Vec3f(1.0f, 1.0f, 0.0f), Vec3f(1.0f, -1.0f, 0.0f), Vec3f(-1.0f, -1.0f, 0.0f)};

	const int n = 20;
	double x = 1.0 - 1.0 / n;
	int tex = screenSize.x / n;
	for(int i = 0; i < n; i++, x -= 2.0 / n)
	{
		double angle = clamp((180.0 + (n - 1) * 10.0) * t - i * 10.0, 0.0, 180.0);
		Mat4 modelview = view;
		modelview.translate(x, 0.0, 0.0);
		modelview.rotate(angle, 0.0, 1.0, 0.0);
		modelview.scale(1.0 / n, 1.0, 1.0);

		const Vec2f uvs[4] = {Vec2f(i * tex + tex, 0.0f), Vec2f(i * tex, 0.0f), Vec2f(i * tex, screenSize.y), Vec2f(i * tex + tex, screenSize.y)};

		// draw the front face
		float c = static_cast<float>(1.0 - angle / 180.0);
		drawImage3D(oldImageID, projection * modelview, face, uvs, Vec4f(c, c, c, 1.0f), true);

		// draw the back face
		modelview.rotate(180.0, 0.0, 1.0, 0.0);
		c = static_cast<float>(angle / 180.0);
		drawImage3D(newImageID, projection * modelview, face, uvs, Vec4f(c, c, c, 1.0f), true);
	}
}
