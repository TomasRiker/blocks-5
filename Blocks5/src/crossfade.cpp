#include "pch.h"
#include "crossfade.h"
#include "engine.h"

Crossfade::Crossfade()
{
	Engine& engine = Engine::inst();
	screenSize = engine.getScreenSize();
	screenPow2Size = engine.getScreenPow2Size();
	screenTexelScale = engine.getFrameCopyRef(0).texelScale;
}

Crossfade::~Crossfade()
{
}

void Crossfade::render(double t,
					   uint oldImageID,
					   uint newImageID)
{
}

RenderState Crossfade::imageState(uint imageID) const
{
	return RenderState(TextureRef(imageID, screenTexelScale), BM_NORMAL);
}

void Crossfade::drawImage(uint imageID,
						  const Vec4f& color) const
{
	const Vec2f corners[4] = {Vec2f(0.0f, 0.0f), Vec2f(screenSize.x, 0.0f),
							  Vec2f(screenSize.x, screenSize.y), Vec2f(0.0f, screenSize.y)};
	Renderer::inst().quad(imageState(imageID), corners, corners, color);
}

void Crossfade::drawColor(const Vec4f& color) const
{
	Renderer& renderer = Renderer::inst();
	renderer.setBlend(BM_NORMAL);
	renderer.rect(Vec2f(0.0f, 0.0f), Vec2f(screenSize.x, screenSize.y), color);
}

void Crossfade::drawImage3D(uint imageID,
							const Mat4& transform,
							const Vec3f* p_corners,
							const Vec2f* p_uvs,
							const Vec4f& color,
							bool cullBackFaces) const
{
	Vertex3 vertices[4];
	for(int i = 0; i < 4; i++)
	{
		vertices[i].position = p_corners[i];
		vertices[i].uv = p_uvs[i];
		vertices[i].color = color;
	}
	Renderer::inst().quads3D(imageState(imageID), transform, vertices, 4, cullBackFaces);
}
