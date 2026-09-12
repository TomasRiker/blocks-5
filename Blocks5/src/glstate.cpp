#include "pch.h"
#include "glstate.h"
#include "engine.h"

namespace GLState
{
	void setTexturing(bool on)
	{
		Engine::inst().flushSprites();
		if(on) glEnable(GL_TEXTURE_2D);
		else glDisable(GL_TEXTURE_2D);
	}

	void bindTexture(GLuint id)
	{
		Engine::inst().flushSprites();
		glBindTexture(GL_TEXTURE_2D, id);
	}

	void pushEnables()
	{
		Engine::inst().flushSprites();
		glPushAttrib(GL_ENABLE_BIT);
	}

	void popEnables()
	{
		// The pop restores texturing, which is state a queued quad reads, so it
		// flushes like the rest. What it restores is whatever the matching push
		// saved, and that is GL's business rather than this file's.
		Engine::inst().flushSprites();
		glPopAttrib();
	}

	void loadTexelMatrix(const Vec2d& texelScale)
	{
		// Column major, and the two zeros in the first two columns are what
		// make the bake in Engine::queueSprite the same arithmetic as this:
		// a texel coordinate is multiplied by the scale and nothing else.
		const GLdouble m[16] = {texelScale.x, 0.0,          0.0, 0.0,
								0.0,          texelScale.y, 0.0, 0.0,
								0.0,          0.0,          1.0, 0.0,
								0.0,          0.0,          0.0, 1.0};
		Engine::inst().flushSprites();
		glPushAttrib(GL_TRANSFORM_BIT);
		glMatrixMode(GL_TEXTURE);
		glLoadMatrixd(m);
		glPopAttrib();
	}

	void pushTextureMatrix()
	{
		Engine::inst().flushSprites();
		glPushAttrib(GL_TRANSFORM_BIT);
		glMatrixMode(GL_TEXTURE);
		glPushMatrix();
		glLoadIdentity();
		glPopAttrib();
	}

	void popTextureMatrix()
	{
		Engine::inst().flushSprites();
		glPushAttrib(GL_TRANSFORM_BIT);
		glMatrixMode(GL_TEXTURE);
		glPopMatrix();
		glPopAttrib();
	}
}
