#include "pch.h"
#include "cf_mosaic.h"
#include "engine.h"

CF_Mosaic::CF_Mosaic()
{
	// Sampled nearest: the blocks are the point.
	bufferID = Engine::inst().createFrameCopyTexture(false, false);
}

CF_Mosaic::~CF_Mosaic()
{
	Renderer::inst().deleteTexture(bufferID);
}

void CF_Mosaic::render(double t,
					   uint oldImageID,
					   uint newImageID)
{
	double x = t - 0.5;
	double s = 0.01 + 3.96 * x * x;
	Vec2i size = s * static_cast<Vec2d>(screenSize);

	Renderer& renderer = Renderer::inst();
	const Vec4f white(1.0f, 1.0f, 1.0f, 1.0f);
	const Vec2f screen[4] = {Vec2f(0.0f, 0.0f), Vec2f(screenSize.x, 0.0f),
							 Vec2f(screenSize.x, screenSize.y), Vec2f(0.0f, screenSize.y)};
	const Vec2f shrunk[4] = {Vec2f(0.0f, 0.0f), Vec2f(size.x, 0.0f), Vec2f(size.x, size.y), Vec2f(0.0f, size.y)};

	// render a shrunk-down version of the image
	renderer.quad(imageState(t <= 0.5 ? oldImageID : newImageID), shrunk, screen, white);

	// copy into the texture
	Engine::inst().captureFrame(bufferID);

	// scale back up to full screen size
	renderer.quad(imageState(bufferID), screen, shrunk, white);
}
