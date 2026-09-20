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
// texels, and the renderer multiplies by texelScale and adds uvOrigin at
// submission, in float - the multiply and the offset a texture matrix would
// do in the vertex stage. An id of 0 is the renderer's own white texel, which
// is what "texturing off" means to a shader.
//
// uvOrigin is where the picture begins inside the GL texture, which is (0, 0)
// for a texture of its own and the corner of its rectangle for one that
// shares a page with others. Callers never see it: they write uv in their own
// picture's texels either way, which is what lets a cache built before the
// move stay valid after it.
//
// tiles says the picture is sampled outside its own edges and relies on
// GL_REPEAT, which is why it cannot share a page: a coordinate past the edge
// would land in whatever was packed next door. It is what a texture was
// declared as at load (Texture::WM_REPEAT), carried to the one place that can
// check it.
//
// uvExtent is how much of the GL texture the picture occupies, (1, 1) for one
// that owns its texture. It exists for Renderer::checkTiling, which has
// nothing else to tell a picture's edge from a page's: with the atlas the two
// stopped being the same thing, and a check written against [0, 1] stopped
// meaning what it says.
struct TextureRef
{
	TextureRef() : id(0), texelScale(1.0f, 1.0f), uvOrigin(0.0f, 0.0f), uvExtent(1.0f, 1.0f), tiles(false) {}
	TextureRef(uint id, const Vec2f& texelScale)
		: id(id), texelScale(texelScale), uvOrigin(0.0f, 0.0f), uvExtent(1.0f, 1.0f), tiles(false) {}
	TextureRef(uint id, const Vec2f& texelScale, const Vec2f& uvOrigin, bool tiles)
		: id(id), texelScale(texelScale), uvOrigin(uvOrigin), uvExtent(1.0f, 1.0f), tiles(tiles) {}
	TextureRef(uint id, const Vec2f& texelScale, const Vec2f& uvOrigin, const Vec2f& uvExtent, bool tiles)
		: id(id), texelScale(texelScale), uvOrigin(uvOrigin), uvExtent(uvExtent), tiles(tiles) {}

	uint id;
	Vec2f texelScale;
	Vec2f uvOrigin;
	Vec2f uvExtent;
	bool tiles;
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

	// The texel scale, the origin and the tiling flag are all functions of
	// the picture and ride along for the bake, so two states are the same
	// state when the id and the blend agree - and two pictures on one atlas
	// page share an id, which is the whole point: they batch together.
	bool operator == (const RenderState& rhs) const
	{
		return texture.id == rhs.texture.id && blend == rhs.blend;
	}
	bool operator != (const RenderState& rhs) const { return !(*this == rhs); }
};

#endif
