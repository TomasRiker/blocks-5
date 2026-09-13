#include "pch.h"
#include "quadarray.h"
#include "engine.h"

// All three state their client arrays in full rather than inheriting them, and
// that includes switching off what they do not use. An array left enabled by an
// earlier draw would be read with this draw's vertex count through a pointer
// belonging to somebody else, which is a wild read and not a wrong picture.
//
// Texturing is the one piece of state they do take from the caller, and it
// arrives there already: every caller of a form carrying texture coordinates
// has just bound a picture, which is what switches texturing on, and the one
// caller of the untextured form is the keycap pass in Font::drawText, which
// switches it off first and says so.
//
// The first two put the sprite batch up before they draw, and that is the
// reason it can be done here rather than at each of their callers: a built
// array is reached as a member or a local through several layers of call, which
// no static check can follow - the same argument LineDrawer::draw makes. The
// third cannot, because it *is* the flush.

void drawQuadArray(const QuadVertex* p_vertices,
				   uint count)
{
	if(!count || !p_vertices) return;
	Engine::inst().flushSprites();

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);

	glVertexPointer(2, GL_FLOAT, sizeof(QuadVertex), &p_vertices->position);
	glTexCoordPointer(2, GL_FLOAT, sizeof(QuadVertex), &p_vertices->uv);
	glDrawArrays(GL_QUADS, 0, static_cast<GLsizei>(count));

	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);
}

void drawQuadArray(const Vec2f* p_positions,
				   uint count)
{
	if(!count || !p_positions) return;
	Engine::inst().flushSprites();

	glEnableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);

	glVertexPointer(2, GL_FLOAT, sizeof(Vec2f), p_positions);
	glDrawArrays(GL_QUADS, 0, static_cast<GLsizei>(count));

	glDisableClientState(GL_VERTEX_ARRAY);
}

// The three attributes must share one stride and it must be no smaller than
// what they occupy, or Emscripten's GL emulation copies the whole array through
// a scratch buffer every draw, and says nothing: its warning for that is behind
// GL_ASSERTIONS, which this build leaves off. Nothing else would report it.
static_assert(sizeof(ColorQuadVertex) == 32, "ColorQuadVertex must stay tightly packed at 32 bytes");

void drawQuadArray(const ColorQuadVertex* p_vertices,
				   uint count)
{
	if(!count || !p_vertices) return;

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);

	glVertexPointer(2, GL_FLOAT, sizeof(ColorQuadVertex), &p_vertices->position);
	glTexCoordPointer(2, GL_FLOAT, sizeof(ColorQuadVertex), &p_vertices->uv);
	glColorPointer(4, GL_FLOAT, sizeof(ColorQuadVertex), &p_vertices->color);
	glDrawArrays(GL_QUADS, 0, static_cast<GLsizei>(count));

	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);
}
