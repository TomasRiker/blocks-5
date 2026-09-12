#include "pch.h"
#include "glstate.h"
#include "engine.h"

GLState::GLState()
	: texture(-1)
	, texturing(-1)
	, texelScale(1.0, 1.0)
{
}

namespace
{
	// One object, in the .cpp: an inline variable in the header is C++17 and
	// this tree builds as C++14 on all three toolchains.
	GLState g_state;
}

namespace GL
{
	void setTexturing(bool on)
	{
		g_state.texturing = on ? 1 : 0;
		Engine::inst().flushSprites();
		if(on) glEnable(GL_TEXTURE_2D);
		else glDisable(GL_TEXTURE_2D);
	}

	void bindTexture(GLuint id,
					 const Vec2d& texelScale)
	{
		// Column major, and the two zeros in the first two columns are what
		// make a bake of the scale into a texture coordinate the same
		// arithmetic as this: a texel coordinate is multiplied by the scale
		// and nothing else.
		const GLdouble m[16] = {texelScale.x, 0.0,          0.0, 0.0,
								0.0,          texelScale.y, 0.0, 0.0,
								0.0,          0.0,          1.0, 0.0,
								0.0,          0.0,          0.0, 1.0};
		g_state.texture = static_cast<GLint>(id);
		g_state.texelScale = texelScale;
		Engine::inst().flushSprites();
		glBindTexture(GL_TEXTURE_2D, id);
		glPushAttrib(GL_TRANSFORM_BIT);
		glMatrixMode(GL_TEXTURE);
		glLoadMatrixd(m);
		glPopAttrib();
	}

	void pushTexturing()
	{
		Engine::inst().flushSprites();
		glPushAttrib(GL_ENABLE_BIT);
	}

	void popTexturing()
	{
		// The pop restores texturing, which is state a queued quad reads, so
		// it flushes like the rest. What it restores is whatever the matching
		// push saved, and that is GL's business rather than this file's -
		// hence the record is forgotten rather than guessed at.
		g_state.texturing = -1;
		Engine::inst().flushSprites();
		glPopAttrib();
	}

	const GLState& state()
	{
		return g_state;
	}
}
