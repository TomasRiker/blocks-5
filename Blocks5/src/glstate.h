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
//
// The record decides: a call that sets what is already set is not issued, and
// a bind that changes nothing does not flush either - which is what
// Engine::renderSprite(Texture*) does over and over with the same texture. It
// is sound only because every door into this state in the tree comes through
// here, which verify.py's gl_doors check is what says, and because a
// test-hooks build reads the real state back on every call and reports a
// record that disagrees.
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
	// glEnable/glDisable(GL_TEXTURE_2D). Does not flush: Engine::flushSprites
	// declares texturing for its own draw, so nothing queued reads this.
	void setTexturing(bool on);

	// Bind to GL_TEXTURE_2D and say how the picture is to be sampled, which
	// goes into the texture matrix. There is no one-argument form on purpose:
	// the two belong together, and a bind that did not say would leave the
	// matrix describing the picture before it.
	//
	// This is the one call here that still flushes, and only where something
	// really moves: what is queued was queued against the binding and the
	// scale about to be replaced.
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

	// Around Engine::flushSprites's own draw. The batch is sprites and a
	// sprite is textured, so the flush says so rather than inheriting whatever
	// stands - and that is what takes texturing out of the batch's state: a
	// setTexturing() then moves nothing a queued quad reads. Neither flushes,
	// for the obvious reason.
	void beginBatchDraw();
	void endBatchDraw();

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

	// What this file believes OpenGL is holding, for a caller that wants to
	// know without asking the driver.
	const GLState& state();

	// How many calls into this file did something and how many did not have to
	// - entry points and not GL calls, which is the one number that means the
	// same on both platforms (a matrix load is four calls on the desktop and
	// two in the browser). That ratio is what the comparison is worth, and the
	// test hook reports both so it can be measured rather than argued about.
	uint callsIssued();
	uint callsSkipped();
	void resetCallCounts();
}

#endif
