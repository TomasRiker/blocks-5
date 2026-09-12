#include "pch.h"
#include "quadarray.h"

// Both of these state what they need rather than inheriting it, and that
// includes switching off what they do not use. An array left enabled by an
// earlier draw would be read with this draw's vertex count through a pointer
// belonging to somebody else, which is a wild read and not a wrong picture.

void drawQuadArray(const QuadVertex* p_vertices,
				   uint count)
{
	if(!count || !p_vertices) return;

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
