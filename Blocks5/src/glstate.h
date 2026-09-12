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
// diagonal scale - a Texture's own 1/w, 1/h, or a screen copy's 1/pow2 with a
// flipped y - and it is a function of the binding and of nothing else, so it
// is kept as the two numbers it is made of and set by the same call that
// binds. Vec2d(1.0, 1.0) is the identity a texture sampled in 0..1 wants. The
// weather is the only thing that asks for more, and it composes its scroll on
// top of the bound picture's own scale inside a balanced push and pop of its
// own.
struct GLState
{
	GLState();

	// -1 in either of the first two means "not known", which is what they
	// start as: nothing may be skipped on the strength of a belief that was
	// never established. A texture of -1 says the scale is unknown as well,
	// there being no unknown Vec2d - which costs nothing, since the two are
	// set by the same call and forgotten by the same one.
	GLint texture;
	int texturing;
	Vec2d texelScale;
};

namespace GL
{
	// glEnable/glDisable(GL_TEXTURE_2D).
	void setTexturing(bool on);

	// Bind to GL_TEXTURE_2D and say how the picture is to be sampled, which
	// goes into the texture matrix. There is no one-argument form on purpose:
	// the two belong together, and a bind that did not say would leave the
	// matrix describing the picture before it.
	//
	// The matrix mode is put back through the attribute stack rather than set
	// to GL_MODELVIEW, because Level::render binds the snow and the clouds
	// with GL_TEXTURE already current.
	void bindTexture(GLuint id, const Vec2d& texelScale);

	// glDeleteTextures for one. GL reverts the binding to 0 where the deleted
	// texture was the bound one, which is knowledge rather than a guess, so
	// that much is recorded - and the matrix is left alone, because a delete
	// does not touch it.
	void deleteTexture(GLuint id);

	// glPushAttrib(GL_ENABLE_BIT) and its pop, which is how lava.cpp and
	// teleporter.cpp put texturing back after drawing their own geometry.
	// Named for the one bit of it the batch cares about, which is also the
	// only bit either of them changes inside the bracket.
	void pushTexturing();
	void popTexturing();

	// Forget the lot, and nothing else: no flush and no GL call, because
	// nothing here moves any state. For a caller that hands GL's own stacks a
	// piece of this state and takes it back again: presentFrame's glPushAttrib bracket
	// restores the binding on the desktop and, in the browser, only the mode
	// and the enables - so what stands afterwards differs per platform and is
	// not worth working out.
	void invalidate();

	// What this file believes OpenGL is holding. Written by every call above
	// and, for now, read by nobody: the proxies still issue every GL call and
	// still flush on every one, so the record cannot yet be wrong in a way
	// that shows. Making it decide anything is a later step, and verify.py's
	// gl_doors check is what says the record is complete enough for it: every
	// raw door into the texture state in the tree is either through here or on
	// a named list with a reason.
	const GLState& state();
}

#endif
