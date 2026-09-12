#ifndef _GLSTATE_H
#define _GLSTATE_H

/*** The GL state a queued sprite depends on ***/

// Three things decide what a batched sprite comes out looking like: which
// texture is bound to GL_TEXTURE_2D, whether texturing is on at all, and what
// stands in the texture matrix. A queued quad is drawn with the state at the
// flush and not at the call, and this game's 2D path has no depth buffer, so
// there is no sorting it out afterwards: whatever moves one of the three has
// to put the batch on the screen first.
//
// These do that, so that the rule lives in one place instead of at a dozen
// call sites where it can be forgotten. verify.py's gl_state check bans the raw
// forms in the sources the batch can reach - an object drawing raw geometry
// straight through glDisable(GL_TEXTURE_2D) is exactly the mistake that leaves
// no trace in a diff, since it puts the sprites of its *neighbours* in the
// wrong place and only on a screen that happens to hold both.
//
// Deliberately not a cache. What was measured, under swiftshader in a browser
// and in a played level, is 28 redundant calls of the 290 the state layer
// issues, out of 4833 in all - before the batch landed. The batch took the
// geometry away rather than the state, so those 28 are a larger share of a much
// smaller number now and still well under the spread between two runs. And it
// would be
// sound only if the fifty-one raw calls in the crossfades, the GUI and the
// credits came through here too - seventy-two across the tree. See ROADMAP 44.
namespace GLState
{
	// glEnable/glDisable(GL_TEXTURE_2D).
	void setTexturing(bool on);

	// Bind to GL_TEXTURE_2D. Texture::bind() is the funnel for the game's own
	// art; this is for a texture GL owns rather than one the Texture class
	// does, which is the hint note's offscreen sheet and nothing else.
	void bindTexture(GLuint id);

	// glPushAttrib(GL_ENABLE_BIT) and its pop, which is how lava.cpp and
	// teleporter.cpp put texturing back after drawing their own geometry.
	void pushEnables();
	void popEnables();

	// The texture matrix - the third of the three, and the one that decides
	// what a queued sprite's texel coordinates mean. loadTexelMatrix() puts a
	// Texture's own 1/w, 1/h there, which is the only absolute matrix this
	// tree ever sets; the push/pop pair lends the stack to a caller that wants
	// something else there for a moment, which is how the hint note samples
	// its own sheet in 0..1 rather than in texels.
	//
	// All three put the matrix mode back through the attribute stack rather
	// than setting GL_MODELVIEW, because Level::render binds the snow and the
	// clouds with GL_TEXTURE already current - and because three functions in
	// one namespace that differ on a thing like that are a trap for whoever
	// adds the fourth.
	void loadTexelMatrix(const Vec2d& texelScale);
	void pushTextureMatrix();
	void popTextureMatrix();
}

#endif
