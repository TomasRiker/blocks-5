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

void CF_Mosaic::render(float t,
					   uint oldImageID,
					   uint newImageID)
{
	float x = t - 0.5f;
	float s = 0.01f + 3.96f * x * x;
	Vec2i size = s * static_cast<Vec2f>(screenSize);

	Renderer& renderer = Renderer::inst();
	const Vec4f white(1.0f, 1.0f, 1.0f, 1.0f);
	const Vec2f whole = static_cast<Vec2f>(screenSize), part = static_cast<Vec2f>(size);
	const Vec2f screen[4] = {Vec2f(0.0f, 0.0f), Vec2f(whole.x, 0.0f), whole, Vec2f(0.0f, whole.y)};
	const Vec2f shrunk[4] = {Vec2f(0.0f, 0.0f), Vec2f(part.x, 0.0f), part, Vec2f(0.0f, part.y)};

	// render a shrunk-down version of the image
	renderer.quad(imageState(t <= 0.5f ? oldImageID : newImageID), shrunk, screen, white);

	// copy into the texture
	Engine::inst().captureFrame(bufferID);

	// scale back up to full screen size
	renderer.quad(imageState(bufferID), screen, shrunk, white);
}
