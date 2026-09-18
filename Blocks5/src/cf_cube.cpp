#include "pch.h"
#include "cf_cube.h"

CF_Cube::CF_Cube()
{
}

CF_Cube::~CF_Cube()
{
}

void CF_Cube::render(float t,
					 uint oldImageID,
					 uint newImageID)
{
	// A cube with the old image on its front and the new one on its left,
	// turned a quarter while the camera pulls back and in again.
	const Mat4 projection = Mat4::perspective(90.0f, 1.0f, 0.1f, 100.0f);
	Mat4 modelview = Mat4::lookAt(0.0f, 0.0f, -2.0f - sinf(t * 3.1415926535897932384626433832795f), 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
	modelview.rotate(90.0f * t, 0.0f, 1.0f, 0.0f);

	Renderer::inst().clear(Vec4f(0.0f, 0.0f, 0.0f, 1.0f));

	// the same face both times, turned by the rotation between the draws;
	// the back faces are culled so the far side never shows through
	const Vec4f white(1.0f, 1.0f, 1.0f, 1.0f);
	const Vec3f face[4] = {Vec3f(-1.0f, 1.0f, -1.0f), Vec3f(1.0f, 1.0f, -1.0f), Vec3f(1.0f, -1.0f, -1.0f), Vec3f(-1.0f, -1.0f, -1.0f)};
	const Vec2f s = static_cast<Vec2f>(screenSize);
	const Vec2f uvs[4] = {Vec2f(s.x, 0.0f), Vec2f(0.0f, 0.0f), Vec2f(0.0f, s.y), s};

	// draw the front face of the cube
	drawImage3D(oldImageID, projection * modelview, face, uvs, white, true);

	// draw the left face of the cube
	modelview.rotate(-90.0f, 0.0f, 1.0f, 0.0f);
	drawImage3D(newImageID, projection * modelview, face, uvs, white, true);
}
