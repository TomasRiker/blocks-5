#ifndef _QUADARRAY_H
#define _QUADARRAY_H

/*** Drawing a built array of quads ***/

// One corner of a quad, as everything in the game that keeps its geometry
// wants it: a position, a texture coordinate, and no colour. Leaving the colour
// out is what lets one built array be drawn several times over under nothing
// but a different glColor - the tile grid's two shadow samples and its picture,
// a string's, the lightning's two passes.
//
// Float and not int, because GL_INT is not a valid vertex attribute type in
// WebGL/GLES2, and a texel and a whole pixel are both well inside float's exact
// range. Double at the call site, so that a caller working in doubles rounds
// once here rather than at every term.
struct QuadVertex
{
	QuadVertex(double px, double py, double u, double v)
		: position(static_cast<float>(px), static_cast<float>(py)),
		  uv(static_cast<float>(u), static_cast<float>(v)) {}

	Vec2f position;
	Vec2f uv;
};

// The same corner with a colour of its own, for an array whose quads do not
// share one. That is what a batch of sprites is: each carries the object's
// tint, its death countdown and whatever the pass multiplied in, so a colour
// held in glColor would flush the batch at every object.
//
// 8 + 8 + 16 = 32 bytes, and the three attributes therefore share a stride
// that is exactly their summed size. Emscripten's GL emulation requires both
// - one stride for all of them, and no smaller than what they occupy - or it
// copies the whole array through a scratch buffer behind a single warning.
struct ColorQuadVertex
{
	Vec2f position;
	Vec2f uv;
	Vec4f color;
};

// Draw count vertices as GL_QUADS out of client memory. Whatever texture the
// caller has bound applies, and so does its glColor.
//
// A vertex buffer object would be the obvious next step and is not one: the
// browser's GL emulation aborts on glDrawArrays(GL_QUADS) with a buffer bound,
// where client arrays go through the same path that immediate mode does.
void drawQuadArray(const QuadVertex* p_vertices, uint count);

// The same for a shape with no texture on it - the keycap frames a font draws
// around a key's name.
void drawQuadArray(const Vec2f* p_positions, uint count);

// And the same for quads carrying their own colours. glColor is ignored for
// the length of the draw and left alone afterwards.
void drawQuadArray(const ColorQuadVertex* p_vertices, uint count);

#endif
