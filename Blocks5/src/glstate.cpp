#include "pch.h"
#include "glstate.h"
#include "renderer.h"

namespace GL
{
	void setTexturing(bool on)
	{
		Renderer::inst().setTexturing(on);
	}

	void bindTexture(GLuint id,
					 const Vec2d& texelScale)
	{
		Renderer::inst().setTexture(TextureRef(id, Vec2f(static_cast<float>(texelScale.x),
														 static_cast<float>(texelScale.y))));
	}

	void deleteTexture(GLuint id)
	{
		Renderer::inst().deleteTexture(id);
	}

	void invalidate()
	{
		Renderer::inst().invalidate();
	}
}
