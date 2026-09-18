#include "pch.h"
#include "cf_camera.h"

CF_Camera::CF_Camera()
{
}

CF_Camera::~CF_Camera()
{
}

void CF_Camera::render(float t,
					   uint oldImageID,
					   uint newImageID)
{
	// The camera looks along the y axis, from the old image up to the new
	// one four units above it.
	const Mat4 projection = Mat4::perspective(90.0f, 1.0f, 0.1f, 100.0f);

	float s1 = sinf(t * 1.5707963267948966192313216916398f);
	float s2 = sinf(t * 3.1415926535897932384626433832795f);
	float y = s1 * s1 * s1 * s1 * s1 * s1 * 4.0f;
	float l = y + s2 * s2 * s2 * s2 * 4.0f;

	const Mat4 transform = projection * Mat4::lookAt(0.0f, y, -1.0f, 0.0f, l, 0.0f, 0.0f, 1.0f, 0.0f);

	Renderer::inst().clear(Vec4f(0.0f, 0.0f, 0.0f, 1.0f));

	const Vec4f white(1.0f, 1.0f, 1.0f, 1.0f);
	const Vec2f s = static_cast<Vec2f>(screenSize);
	const Vec2f uvs[4] = {Vec2f(s.x, 0.0f), Vec2f(0.0f, 0.0f), Vec2f(0.0f, s.y), s};

	// draw the old image
	const Vec3f oldCorners[4] = {Vec3f(-1.0f, 1.0f, 0.0f), Vec3f(1.0f, 1.0f, 0.0f), Vec3f(1.0f, -1.0f, 0.0f), Vec3f(-1.0f, -1.0f, 0.0f)};
	drawImage3D(oldImageID, transform, oldCorners, uvs, white, false);

	// draw the new image
	const Vec3f newCorners[4] = {Vec3f(-1.0f, 5.0f, 0.0f), Vec3f(1.0f, 5.0f, 0.0f), Vec3f(1.0f, 3.0f, 0.0f), Vec3f(-1.0f, 3.0f, 0.0f)};
	drawImage3D(newImageID, transform, newCorners, uvs, white, false);
}
