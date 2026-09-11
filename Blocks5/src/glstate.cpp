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
}
