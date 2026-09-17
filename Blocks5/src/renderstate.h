#ifndef _RENDERSTATE_H
#define _RENDERSTATE_H

/*** What a quad is drawn under, as far as it can differ from quad to quad ***/

// Two things alternate from one quad to the next inside a render pass: the
// texture it samples and the blend it is composited with. Everything rarer -
// the colour mask, the stencil, the scissor, the target - is a bracket around
// a whole group of quads and lives as a scope on the Renderer (renderer.h),
// so that this stays two fields and comparing two states stays one 64-bit
// compare.

// A GL texture and how its texels map onto uv: every caller writes uv in
// texels, and the renderer multiplies by this at submission, in float - the
// multiply a texture matrix would do in the vertex stage. An id of 0 is the
// renderer's own white texel, which is what "texturing off" means to a
// shader.
struct TextureRef
{
	TextureRef() : id(0), texelScale(1.0f, 1.0f) {}
	TextureRef(uint id, const Vec2f& texelScale) : id(id), texelScale(texelScale) {}

	uint id;
	Vec2f texelScale;
};

// The eight blend functions the tree uses, by name: a mode maps onto one
// glBlendFuncSeparate quadruple in renderer.cpp, and a quadruple that is not
// one of these is a bug that says so rather than a ninth mode.
enum BlendMode
{
	// colour factors / alpha factors
	BM_NORMAL,        // SRC_ALPHA, ONE_MINUS_SRC_ALPHA / ONE, ONE
	BM_ADDITIVE,      // SRC_ALPHA, ONE / ONE, ONE
	BM_MULTIPLY,      // DST_COLOR, ZERO / ONE, ONE
	BM_ZERO,          // ZERO, ZERO / ZERO, ZERO
	BM_ADD_ALL,       // ONE, ONE / ONE, ONE
	BM_DARKEN_UNLIT,  // ONE_MINUS_DST_ALPHA, DST_ALPHA / ONE, ONE
	BM_BAKE,          // SRC_ALPHA, ONE_MINUS_SRC_ALPHA / ONE, ONE_MINUS_SRC_ALPHA
	BM_PREMULTIPLIED, // ONE, ONE_MINUS_SRC_ALPHA / ONE, ONE_MINUS_SRC_ALPHA
	BM_COUNT
};

struct RenderState
{
	RenderState() : blend(BM_NORMAL) {}
	RenderState(const TextureRef& texture, BlendMode blend) : texture(texture), blend(blend) {}

	TextureRef texture;
	BlendMode blend;

	RenderState with(BlendMode other) const { return RenderState(texture, other); }

	// The texel scale is a function of the id and rides along for the bake,
	// so two states are the same state when the id and the blend agree.
	bool operator == (const RenderState& rhs) const
	{
		return texture.id == rhs.texture.id && blend == rhs.blend;
	}
	bool operator != (const RenderState& rhs) const { return !(*this == rhs); }
};

#endif
