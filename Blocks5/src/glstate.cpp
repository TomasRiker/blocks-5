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

	void loadTextureMatrix(const GLdouble* p_matrix)
	{
		Engine::inst().flushSprites();
		glPushAttrib(GL_TRANSFORM_BIT);
		glMatrixMode(GL_TEXTURE);
		glLoadMatrixd(p_matrix);
		glPopAttrib();
	}

	void pushTextureMatrix()
	{
		Engine::inst().flushSprites();
		glMatrixMode(GL_TEXTURE);
		glPushMatrix();
		glLoadIdentity();
		glMatrixMode(GL_MODELVIEW);
	}

	void popTextureMatrix()
	{
		Engine::inst().flushSprites();
		glMatrixMode(GL_TEXTURE);
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);
	}
}
