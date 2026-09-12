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

	uint g_issued = 0;
	uint g_skipped = 0;

	// The texture matrix a texel scale means, and the only one this tree ever
	// sets absolutely: a diagonal. Column major, and the two zeros in the first
	// two columns are what make a bake of the scale into a texture coordinate
	// the same arithmetic - a texel coordinate is multiplied by the scale and
	// nothing else.
	void texelMatrix(const Vec2d& texelScale,
					 GLdouble* p_out)
	{
		for(int i = 0; i < 16; i++) p_out[i] = 0.0;
		p_out[0] = texelScale.x;
		p_out[5] = texelScale.y;
		p_out[10] = 1.0;
		p_out[15] = 1.0;
	}

	// Read back what OpenGL is really holding and say so where the record
	// disagrees. A record that decides has to be checked: the first entry that
	// says the wrong thing is a wrong picture rather than a missing
	// optimisation, and the failure is a frame nobody was looking at. Only what
	// is believed - a -1 says nothing and cannot be wrong.
	//
	// In a native test-hooks build and nowhere else. It is a glGet per state
	// call, which is affordable under Xvfb where LinuxBuild/test/frames.sh puts
	// every onRender in the tree through it - and in the browser the same line
	// is a WebGL getParameter per call, which is larger than everything
	// WebBuild/test/perf.js measures. What it looks for is the tree's own code
	// and that is the same on both platforms, so the native run answers for
	// both; for what only WebBuild can do wrong there is verify.py's gl_doors,
	// which reads those sources too.
	void checkRecord(const char* p_where)
	{
#if defined(BLOCKS5_TEST_HOOKS) && !defined(__EMSCRIPTEN__)
		if(g_state.texture >= 0)
		{
			GLint texture = 0;
			glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
			if(texture != g_state.texture)
			{
				printfLog("+ ERROR: GL::%s believes texture %d is bound; GL says %d.\n",
						  p_where, static_cast<int>(g_state.texture),
						  static_cast<int>(texture));
			}

			// All sixteen and not the two on the diagonal: a scroll left in the
			// matrix by the weather would be an offset, which is exactly the
			// mistake worth catching and lives outside them.
			GLdouble real[16], want[16];
			glGetDoublev(GL_TEXTURE_MATRIX, real);
			texelMatrix(g_state.texelScale, want);
			if(memcmp(real, want, sizeof(want)))
			{
				printfLog("+ ERROR: GL::%s believes the texel scale is %g, %g; "
						  "GL holds %g, %g%s.\n", p_where,
						  g_state.texelScale.x, g_state.texelScale.y,
						  real[0], real[5],
						  (real[12] || real[13]) ? " with an offset" : "");
			}
		}

		if(g_state.texturing >= 0)
		{
			const int texturing = (glIsEnabled(GL_TEXTURE_2D) == GL_TRUE) ? 1 : 0;
			if(texturing != g_state.texturing)
			{
				printfLog("+ ERROR: GL::%s believes texturing is %s; GL says it is %s.\n",
						  p_where, g_state.texturing ? "on" : "off",
						  texturing ? "on" : "off");
			}
		}
#else
		(void)p_where;
#endif
	}
}

namespace GL
{
	void setTexturing(bool on)
	{
		checkRecord("setTexturing");

		const int want = on ? 1 : 0;
		if(g_state.texturing == want) { g_skipped++; return; }
		g_issued++;
		g_state.texturing = want;

		// No flush, and that is the point of GL::beginBatchDraw: the flush
		// declares texturing for its own draw, so nothing queued reads this.
		if(on) glEnable(GL_TEXTURE_2D);
		else glDisable(GL_TEXTURE_2D);
	}

	void bindTexture(GLuint id,
					 const Vec2d& texelScale)
	{
		checkRecord("bindTexture");

		// The scale is known only while the binding is, because it is a
		// function of it: where the binding is unknown the matrix has to be
		// set whatever the two numbers happen to say.
		const bool known = (g_state.texture >= 0);
		const bool sameTexture = known && (g_state.texture == static_cast<GLint>(id));
		const bool sameScale = known && (g_state.texelScale == texelScale);
		if(sameTexture && sameScale) { g_skipped++; return; }
		g_issued++;

		// Whatever is queued was queued against the binding and the scale
		// about to be replaced. This is the one flush left in this file that
		// the batch actually needs.
		Engine::inst().flushSprites();

		if(!sameTexture)
		{
			g_state.texture = static_cast<GLint>(id);
			glBindTexture(GL_TEXTURE_2D, id);
		}

		if(!sameScale)
		{
			GLdouble m[16];
			texelMatrix(texelScale, m);
			g_state.texelScale = texelScale;
			glPushAttrib(GL_TRANSFORM_BIT);
			glMatrixMode(GL_TEXTURE);
			glLoadMatrixd(m);
			glPopAttrib();
		}
	}

	void deleteTexture(GLuint id)
	{
		checkRecord("deleteTexture");

		// Flushes like the rest, and for the same reason: whatever is queued
		// may be about to be drawn out of the texture being thrown away.
		Engine::inst().flushSprites();
		if(g_state.texture == static_cast<GLint>(id)) g_state.texture = 0;
		glDeleteTextures(1, &id);
	}

	void beginBatchDraw()
	{
		if(g_state.texturing == 1) return;
		glEnable(GL_TEXTURE_2D);
		// Where the record knew nothing, the enable establishes it and there
		// is nothing to put back. Where it knew texturing was off, that is the
		// game's wish and endBatchDraw() restores it.
		if(g_state.texturing < 0) g_state.texturing = 1;
	}

	void endBatchDraw()
	{
		if(g_state.texturing == 0) glDisable(GL_TEXTURE_2D);
	}

	void pushTexturing()
	{
		// Changes nothing, so there is nothing to put up first.
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

	void invalidate()
	{
		g_state.texture = -1;
		g_state.texturing = -1;
	}

	const GLState& state()
	{
		return g_state;
	}

	uint callsIssued()
	{
		return g_issued;
	}

	uint callsSkipped()
	{
		return g_skipped;
	}

	void resetCallCounts()
	{
		g_issued = 0;
		g_skipped = 0;
	}
}
