#ifndef _GLSTATE_H
#define _GLSTATE_H

/*** The texture state, forwarded to the renderer ***/

// What GL:: kept - the record of the binding, of the texturing enable and of
// the texel scale, and the flushes a change to them cost - lives in Renderer
// now (renderer.h). These four names stay for the sources that still draw
// raw, which is most of the tree outside the level: each is one forwarding
// call, so that a raw screen and a batched level share one record.
// RENDERER-REDESIGN.md's stage 2 deletes this file with the last glBegin.
namespace GL
{
	// Renderer::setTexturing: "off" is the renderer's white texel, and the
	// bound texture is remembered across it.
	void setTexturing(bool on);

	// Renderer::setTexture. Binds for real whatever the mode, so that an
	// upload or a copy right after it lands in this texture, and inside a
	// DirectGL sets the texture matrix so that raw texel coordinates read in
	// the picture's own texels. There is no one-argument form on purpose:
	// the two belong together.
	void bindTexture(GLuint id, const Vec2d& texelScale);

	// Renderer::deleteTexture, which flushes first.
	void deleteTexture(GLuint id);

	// Renderer::invalidate, for the one caller that hands GL's own stacks
	// this state and takes it back again: presentFrame's glPushAttrib
	// bracket restores the binding on the desktop and, in the browser, only
	// the mode and the enables - so what stands afterwards differs per
	// platform and is not worth working out.
	void invalidate();
}

#endif
