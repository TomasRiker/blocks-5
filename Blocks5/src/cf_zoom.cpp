#include "pch.h"
#include "cf_zoom.h"

CF_Zoom::CF_Zoom(const Vec2i& targetIn,
				 const Vec2i& targetOut) : targetIn(targetIn), targetOut(targetOut)
{
}

CF_Zoom::~CF_Zoom()
{
}

void CF_Zoom::render(float t,
					 uint oldImageID,
					 uint newImageID)
{
	Renderer::inst().clear(Vec4f(0.0f, 0.0f, 0.0f, 1.0f));

	Vec2f targetPos;

	uint imageID;
	if(t <= 0.5f)
	{
		t = 2.0f * t;
		imageID = oldImageID;
		targetPos = Vec2f(-1.0f, -1.0f) + static_cast<Vec2f>(2 * targetIn) / screenSize;
	}
	else
	{
		t = 2.0f - 2.0f * t;
		imageID = newImageID;
		targetPos = Vec2f(-1.0f, -1.0f) + static_cast<Vec2f>(2 * targetOut) / screenSize;
	}

	// Twenty-five copies of the image a step apart along the zoom, each
	// fainter than the last, which is what smears it.
	const Vec3f corners[4] = {Vec3f(-1.0f, -1.0f, 0.0f), Vec3f(1.0f, -1.0f, 0.0f), Vec3f(1.0f, 1.0f, 0.0f), Vec3f(-1.0f, 1.0f, 0.0f)};
	const Vec2f s = static_cast<Vec2f>(screenSize);
	const Vec2f uvs[4] = {Vec2f(0.0f, 0.0f), Vec2f(s.x, 0.0f), s, Vec2f(0.0f, s.y)};

	float ts = t - 25.0f * 0.01f;
	for(int i = 0; i < 25; i++)
	{
		// the copy's own place along the zoom; t stays the transition's
		const float tc = clamp(ts, 0.0f, 1.0f);

		float fov = 90.0f - tc * tc * 80.0f;
		Mat4 projection = Mat4::perspective(fov, 1.0f, 0.001f, 10.0f);
		projection.translate(1.0f / screenSize.x, -1.0f / screenSize.y, 0.0f);

		float s;
		if(tc > 0.81649f) s = 1.0f;
		else s = sinf(1.5f * tc * tc * 1.5707963267948966192313216916398f);
		Vec2f camPos = s * targetPos;
		float z = -1.0f + 0.99f * tc;
		float r = tc * tc * 1.5f;
		const Mat4 modelview = Mat4::lookAt(camPos.x, camPos.y, z, camPos.x, camPos.y, 0.0f, -sinf(r), -cosf(r), 0.0f);

		// draw the image
		drawImage3D(imageID, projection * modelview, corners, uvs,
					Vec4f(1.0f, 1.0f, 1.0f, 1.0f - 0.5f * tc * tc), false);

		ts += 0.01f;
	}

	// draw the colour quad
	drawColor(Vec4f(1.0f, 1.0f, 1.0f, t * t));
}
