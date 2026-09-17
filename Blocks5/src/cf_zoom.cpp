#include "pch.h"
#include "cf_zoom.h"

CF_Zoom::CF_Zoom(const Vec2i& targetIn,
				 const Vec2i& targetOut) : targetIn(targetIn), targetOut(targetOut)
{
}

CF_Zoom::~CF_Zoom()
{
}

void CF_Zoom::render(double t,
					 uint oldImageID,
					 uint newImageID)
{
	Renderer::inst().clear(Vec4f(0.0f, 0.0f, 0.0f, 1.0f));

	Vec2d targetPos;

	uint imageID;
	if(t <= 0.5)
	{
		t = 2.0 * t;
		imageID = oldImageID;
		targetPos = Vec2d(-1.0, -1.0) + static_cast<Vec2d>(2 * targetIn) / screenSize;
	}
	else
	{
		t = 2.0 - 2.0 * t;
		imageID = newImageID;
		targetPos = Vec2d(-1.0, -1.0) + static_cast<Vec2d>(2 * targetOut) / screenSize;
	}

	// Twenty-five copies of the image a step apart along the zoom, each
	// fainter than the last, which is what smears it.
	const Vec3f corners[4] = {Vec3f(-1.0f, -1.0f, 0.0f), Vec3f(1.0f, -1.0f, 0.0f), Vec3f(1.0f, 1.0f, 0.0f), Vec3f(-1.0f, 1.0f, 0.0f)};
	const Vec2f s = static_cast<Vec2f>(screenSize);
	const Vec2f uvs[4] = {Vec2f(0.0f, 0.0f), Vec2f(s.x, 0.0f), s, Vec2f(0.0f, s.y)};

	double ts = t - 25.0 * 0.01;
	for(int i = 0; i < 25; i++)
	{
		t = clamp(ts, 0.0, 1.0);

		double fov = 90.0 - t * t * 80.0;
		Mat4 projection = Mat4::perspective(fov, 1.0, 0.001, 10.0);
		projection.translate(1.0 / screenSize.x, -1.0 / screenSize.y, 0.0);

		double s;
		if(t > 0.81649) s = 1.0;
		else s = sin(1.5 * t * t * 1.5707963267948966192313216916398);
		Vec2d camPos = s * targetPos;
		double z = -1.0 + 0.99 * t;
		double r = t * t * 1.5;
		const Mat4 modelview = Mat4::lookAt(camPos.x, camPos.y, z, camPos.x, camPos.y, 0.0, -sin(r), -cos(r), 0.0);

		// draw the image
		drawImage3D(imageID, projection * modelview, corners, uvs,
					Vec4f(1.0f, 1.0f, 1.0f, static_cast<float>(1.0 - 0.5 * t * t)), false);

		ts += 0.01;
	}

	// draw the colour quad
	drawColor(Vec4f(1.0f, 1.0f, 1.0f, static_cast<float>(t * t)));
}
