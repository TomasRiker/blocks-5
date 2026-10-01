#ifndef _RENDERSTATE_H
#define _RENDERSTATE_H

/*** What a quad is drawn under, as far as it can differ from quad to quad ***/

// Only the texture and the blend alternate from quad to quad inside a pass.
// Everything rarer - colour mask, stencil, scissor, target - is a scope on the
// Renderer (renderer.h) around a group of quads, so this stays two fields and
// an equality test two compares.

// A GL texture and how its texels map onto uv. Callers write uv in their own
// picture's texels; Renderer::pushQuad multiplies by texelScale and adds
// uvOrigin, in float. An id of 0 is no texture at all (a default ref, or one
// whose texture was deleted), not a stand-in: flat geometry samples the
// renderer's built-in block and disc, which sit in an atlas page and carry its
// id. Renderer::checkTiling skips 0, having no picture edge to check against.
//
// uvOrigin is where the picture starts inside the GL texture: (0, 0) for a
// texture of its own, its rectangle's corner in a shared page. Callers never
// see it, which keeps a cache of uv valid when a repack moves the picture.
//
// tiles: the picture is sampled past its edges and relies on GL_REPEAT
// (declared Texture::WM_REPEAT), which wraps at the texture's edge, so it
// cannot share a page. uvExtent is how much of the GL texture the picture
// covers, (1, 1) for a texture of its own; in a page the picture's edge is not
// the texture's, and checkTiling needs it.
//
// Only Texture::ref() hands out a ref into a page, through the constructor
// with an extent. The shorter ones leave (1, 1), right for their users: a
// render target read back, and a picture with a texture to itself.
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

	// Scale, origin, extent and tiling follow from the picture and ride along
	// for the bake and the check. Only id and blend decide, so two pictures on
	// one atlas page are one state and batch together.
	bool operator == (const RenderState& rhs) const
	{
		return texture.id == rhs.texture.id && blend == rhs.blend;
	}
	bool operator != (const RenderState& rhs) const { return !(*this == rhs); }
};

#endif
