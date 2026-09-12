#ifndef _GLSTATE_H
#define _GLSTATE_H

/*** What OpenGL is holding, and the proxies that put it there ***/

// Three things decide what a batched sprite comes out looking like: which
// texture is bound to GL_TEXTURE_2D, whether texturing is on at all, and what
// stands in the texture matrix. A queued quad is drawn with the state at the
// flush and not at the call, and this game's 2D path has no depth buffer, so
// there is no sorting it out afterwards: whatever moves one of the three has
// to put the batch on the screen first.
//
// The third is not sixteen doubles. Every absolute matrix this tree sets is a
// diagonal scale - a Texture's own 1/w, 1/h, or a framebuffer copy's
// 1/pow2 - and it is a function of the binding and of nothing else, so it is
// kept as the two numbers it is made of. Vec2d(1.0, 1.0) is the identity a
// texture sampled in 0..1 wants. The weather is the only thing that asks for
// more, and it composes its scroll on top of the bound picture's own scale
// inside a balanced push and pop of its own.
struct GLState
{
	GLState();

	// -1 in either of the first two means "not known", which is what they
	// start as: nothing may be skipped on the strength of a belief that was
	// never established.
	GLint texture;
	int texturing;
	Vec2d texelScale;
};

namespace GL
{
	// glEnable/glDisable(GL_TEXTURE_2D).
	void setTexturing(bool on);

	// Bind to GL_TEXTURE_2D. Texture::bind() is the funnel for the game's own
	// art; this is for a texture GL owns rather than one the Texture class
	// does, which is the hint note's offscreen sheet and nothing else.
	void bindTexture(GLuint id);

	// glPushAttrib(GL_ENABLE_BIT) and its pop, which is how lava.cpp and
	// teleporter.cpp put texturing back after drawing their own geometry.
	// Named for the one bit of it the batch cares about, which is also the
	// only bit either of them changes inside the bracket.
	void pushTexturing();
	void popTexturing();

	// The texture matrix. loadTexelMatrix() puts a Texture's own 1/w, 1/h
	// there; the push/pop pair lends the stack to a caller that wants
	// something else for a moment, which is how the hint note samples its own
	// sheet in 0..1 rather than in texels.
	//
	// All three put the matrix mode back through the attribute stack rather
	// than setting GL_MODELVIEW, because Level::render binds the snow and the
	// clouds with GL_TEXTURE already current - and because three functions in
	// one namespace that differ on a thing like that are a trap for whoever
	// adds the fourth.
	void loadTexelMatrix(const Vec2d& texelScale);
	void pushTextureMatrix();
	void popTextureMatrix();

	// What this file believes OpenGL is holding. Written by every call above
	// and, for now, read by nobody: the proxies still issue every GL call and
	// still flush on every one, so the record cannot yet be wrong in a way
	// that shows. Making it decide anything is a later step, and it is only
	// sound once every raw glBindTexture and GL_TEXTURE_2D enable in the tree
	// comes through here.
	const GLState& state();
}

#endif
