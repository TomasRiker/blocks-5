#include "pch.h"
#include "cf_slices.h"

CF_Slices::CF_Slices()
{
}

CF_Slices::~CF_Slices()
{
}

void CF_Slices::render(float t,
					   uint oldImageID,
					   uint newImageID)
{
	// Twenty vertical slices, each with the old image on its front and the
	// new one on its back, turning over one after another.
	const Mat4 projection = Mat4::perspective(90.0f, 1.0f, 0.1f, 100.0f);
	const Mat4 view = Mat4::lookAt(0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);

	Renderer::inst().clear(Vec4f(0.0f, 0.0f, 0.0f, 1.0f));

	const Vec3f face[4] = {Vec3f(-1.0f, 1.0f, 0.0f), Vec3f(1.0f, 1.0f, 0.0f), Vec3f(1.0f, -1.0f, 0.0f), Vec3f(-1.0f, -1.0f, 0.0f)};

	const int n = 20;
	float x = 1.0f - 1.0f / n;
	int tex = screenSize.x / n;
	for(int i = 0; i < n; i++, x -= 2.0f / n)
	{
		float angle = clamp((180.0f + (n - 1) * 10.0f) * t - i * 10.0f, 0.0f, 180.0f);
		Mat4 modelview = view;
		modelview.translate(x, 0.0f, 0.0f);
		modelview.rotate(angle, 0.0f, 1.0f, 0.0f);
		modelview.scale(1.0f / n, 1.0f, 1.0f);

		const float left = static_cast<float>(i * tex), right = static_cast<float>(i * tex + tex), bottom = static_cast<float>(screenSize.y);
		const Vec2f uvs[4] = {Vec2f(right, 0.0f), Vec2f(left, 0.0f), Vec2f(left, bottom), Vec2f(right, bottom)};

		// draw the front face
		float c = 1.0f - angle / 180.0f;
		drawImage3D(oldImageID, projection * modelview, face, uvs, Vec4f(c, c, c, 1.0f), true);

		// draw the back face
		modelview.rotate(180.0f, 0.0f, 1.0f, 0.0f);
		c = angle / 180.0f;
		drawImage3D(newImageID, projection * modelview, face, uvs, Vec4f(c, c, c, 1.0f), true);
	}
}
