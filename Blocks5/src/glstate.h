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
// Deliberately not a cache. Skipping a call that sets what is already set is
// worth about four percent of what the game asks of the driver once the sprite
// batch has taken its share, which is well under the spread between two runs -
// and it would be sound only if every one of the hundred-odd raw calls in the
// crossfades, the GUI and the credits came through here too. See ROADMAP 44.
namespace GLState
{
	// glEnable/glDisable(GL_TEXTURE_2D).
	void setTexturing(bool on);

	// Bind to GL_TEXTURE_2D. For a texture GL owns rather than one the Texture
	// class does - the hint note's offscreen sheet, the credits' buffer.
	void bindTexture(GLuint id);

	// glPushAttrib(GL_ENABLE_BIT) and its pop, which is how lava.cpp and
	// teleporter.cpp put texturing back after drawing their own geometry.
	void pushEnables();
	void popEnables();
}

#endif
