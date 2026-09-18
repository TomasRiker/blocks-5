#ifndef _RENDERER_H
#define _RENDERER_H

/*** The one path every quad takes to the screen ***/

// Everything the game draws goes through here: a quad is baked - transform,
// uv, colour - into a stream of 32-byte vertices and drawn with the next
// flush, which happens only where the state a quad is drawn under changes
// (the texture or the blend, see renderstate.h), where a scope begins or
// ends, where the stream is full, or where raw GL is about to read or
// replace what was drawn. .claude/rules/rendering.md has the design;
// renderer.cpp carries the reasons.

#include "renderstate.h"

// The stream vertex: 32 bytes, 2D, transform applied and uv normalised.
struct Vertex
{
	Vec2f position;
	Vec2f uv;
	Vec4f color;
};

// The 3D vertex of the crossfades and the credits: 36 bytes, drawn through
// quads3D under a matrix of the caller's own.
struct Vertex3
{
	Vec3f position;
	Vec2f uv;
	Vec4f color;
};

// The cached vertex: position and uv in texels, no colour, 16 bytes, so one
// built array can be drawn several times under another colour and offset.
struct QuadVertex
{
	QuadVertex(float px, float py, float u, float v)
		: position(static_cast<float>(px), static_cast<float>(py)),
		  uv(static_cast<float>(u), static_cast<float>(v)) {}

	Vec2f position;
	Vec2f uv;
};

class Renderer : public Singleton<Renderer>
{
	friend class Singleton<Renderer>;

public:
	// Why a flush happened, for the histogram the test hook reports.
	enum FlushReason
	{
		FR_TEXTURE,   // the next quad samples another texture
		FR_BLEND,     // the next quad blends differently
		FR_SCOPE,     // a scope began or ended
		FR_FULL,      // the stream held its 16384 quads
		FR_EXPLICIT,  // a clear, a copy, a 3D draw, a target switch
		FR_FRAME,     // the end of the frame
		FR_DIRECT,    // a DirectGL bracket opening
		FR_COUNT
	};

	struct Stats
	{
		uint flushes;               // every flush, drawing or not
		uint draws;                 // the flushes that drew
		uint quads;                 // quads those draws put up
		uint drawsByReason[FR_COUNT];
	};

	// Once the GL context stands, after GLExtensions::init; stops the game
	// where the program will not link.
	void init();

	// Per frame: the frame's projection, the stack on the identity, no
	// scissor.
	void frameBegin(const Vec2i& size);
	void frameEnd();

	// A target of another size for the length of a bake, which
	// Engine::beginRenderToTexture binds the framebuffer around: its
	// projection, an identity transform on top of the stack, the scissor
	// suspended; endTarget puts all three back. Both flush.
	void beginTarget(const Vec2i& size);
	void endTarget();

	// --- the state a quad is drawn under ---------------------------------
	//
	// setTexture binds for real as well, so that a raw upload or copy right
	// after it lands in that texture.
	void setTexture(const TextureRef& texture);
	void setBlend(BlendMode blend);
	const RenderState& state() const { return current; }

	// glDeleteTextures for one, after a flush.
	void deleteTexture(uint id);

	// --- the transform, baked on the CPU -----------------------------------
	//
	// A 2D affine stack, applied at submission, so a change of it never
	// breaks a batch.
	void push();
	void pop();
	void translate(float x, float y);
	void scale(float x, float y);
	void rotate(float degrees);
	void loadIdentity();

	// --- drawing -------------------------------------------------------------
	//
	// Positions in the caller's space, uv in texels of the state's texture,
	// colours as given (the program clamps them to 0..1).

	// A sprite as Engine::renderSprite defines it, under the current state.
	void sprite(const Vec2f& position, const Vec2i& halfSize, const Vec2i& otherHalf,
				int u0, int u1, int v0, int v1,
				const Vec4f& color, float rotation, float scaling);

	// A built array of quads under one colour: the tile grid, the font, the
	// GUI's frames.
	void quads(const RenderState& s, const QuadVertex* p_vertices, uint count, const Vec4f& color);

	// The same from positions alone, flat: the font's keycap frames.
	void quads(const Vec2f* p_positions, uint count, const Vec4f& color);

	// Quads with a colour per vertex: the particle systems, the toxic grid.
	void quads(const RenderState& s, const Vertex* p_vertices, uint count);

	// A quad whose texture coordinates go through a matrix first, in the
	// float arithmetic GL's texture matrix used: the weather and the menu's
	// clouds scroll that way. The matrix starts from the texture's texel
	// scale (Mat4::scaling), as the matrix under a bind did.
	void scrolledQuad(uint textureId, const Mat4& textureMatrix, const Vec2f* p_corners, const Vec2f* p_uvs, const Vec4f& color);

	// One quad from four corners in order, one colour or a colour a corner;
	// the last is flat with a colour a corner, the GUI's gradients.
	void quad(const RenderState& s, const Vec2f* p_corners, const Vec2f* p_uvs, const Vec4f& color);
	void quad(const RenderState& s, const Vec2f* p_corners, const Vec2f* p_uvs, const Vec4f* p_colors);
	void quad(const Vec2f* p_corners, const Vec4f* p_colors);

	// Triangles, three vertices each: the hint's note mesh, the star wipe,
	// the scrollbar arrows.
	void triangles(const RenderState& s, const Vertex* p_vertices, uint count);
	void triangles(const Vec2f* p_positions, const Vec4f* p_colors, uint count);

	// Quads in 3D under a matrix of the caller's own, projection included
	// (vec.h's Mat4 builds one as GL did), each call one draw: the four 3D
	// crossfades and the credits' stars. The 2D stack does not apply.
	void quads3D(const RenderState& s, const Mat4& transform, const Vertex3* p_vertices, uint count, bool cullBackFaces);

	// Flat geometry, all under the current blend; a point is the disc of
	// the built-in texture.
	void rect(const Vec2f& min, const Vec2f& max, const Vec4f& color);
	void line(const Vec2f& a, const Vec2f& b, float width, const Vec4f& color);
	void polyline(const std::vector<Vec2f>& points, float width, const Vec4f& color, bool closed = false);
	void point(const Vec2f& p, float size, const Vec4f& color);

	// A one-pixel line on the pixels GL's rasterizer lit for it, and the
	// four of a rectangle's outline; the colour runs from a to b.
	void hairline(const Vec2f& a, const Vec2f& b, const Vec4f& colorA, const Vec4f& colorB);
	void hairline(const Vec2f& a, const Vec2f& b, const Vec4f& color) { hairline(a, b, color, color); }
	void hairlineRect(const Vec2f& min, const Vec2f& max, const Vec4f& color);

	// A dashed polyline: `on` units drawn, `off` skipped, from `phase`
	// along the path - the editor's marching ants.
	void dashes(const std::vector<Vec2f>& points, float width, const Vec4f& color,
				float on, float off, float phase, bool closed);

	// Clears flush first, because they replace what was drawn; the scissor
	// and the mask apply to them as to a draw.
	void clear(const Vec4f& color);
	void clearStencil();

	// The target's pixels from its origin, size wide, into a texture at
	// destination, after a flush.
	void copyFrame(uint textureId, const Vec2i& destination, const Vec2i& size);

	// --- the rare state, as scopes ------------------------------------------
	//
	// Each flushes at both ends and puts the previous value back. Drawing
	// inside one is ordinary batched drawing.

	class ColorMaskScope
	{
	public:
		ColorMaskScope(bool r, bool g, bool b, bool a);
		~ColorMaskScope();
	private:
		bool previous[4];
	};

	// Write ref into the stencil wherever a fragment lands (ALWAYS/REPLACE).
	class StencilWriteScope
	{
	public:
		explicit StencilWriteScope(int ref);
		~StencilWriteScope();
	};

	// Draw only where the stencil holds ref (EQUAL/KEEP).
	class StencilTestScope
	{
	public:
		explicit StencilTestScope(int ref);
		~StencilTestScope();
	};

	// Discard fragments whose alpha is 0 before they touch the stencil.
	class DiscardTransparentScope
	{
	public:
		DiscardTransparentScope();
		~DiscardTransparentScope();
	};

	// Clip to a rectangle of the target, top-left origin; one inside
	// another clips to their intersection.
	class ScissorScope
	{
	public:
		ScissorScope(const Vec2i& position, const Vec2i& size);
		~ScissorScope();
	private:
		bool previousOn;
		Vec2i previousPosition;
		Vec2i previousSize;
	};

	// Raw GL for the length of a bracket: the constructor flushes, so that
	// what the raw code reads or replaces is on the target, and the
	// destructor makes the renderer forget what GL holds.
	class DirectGL
	{
	public:
		DirectGL();
		~DirectGL();
	};

	// --- measuring ----------------------------------------------------------
	const Stats& stats() const { return counters; }
	void resetStats();

	// -flushall: a flush after every quad.
	void setFlushAll(bool on) { flushAll = on; }

private:
	Renderer();
	~Renderer();

	// Put up everything queued, under the state it was queued against. Every
	// flush is one of this file's own: a state change, a scope, a full
	// stream, a clear, a copy, a 3D draw, a target switch, the frame's end
	// or a DirectGL bracket opening - nothing outside asks for one.
	void flush(FlushReason reason = FR_EXPLICIT);

	// Forget what GL is holding, and nothing else; the bracket's destructor.
	void invalidate();

	// x' = m00 * x + m01 * y + tx; y' = m10 * x + m11 * y + ty. Float, as
	// GL's matrix stack was.
	struct Transform
	{
		float m00, m01, m10, m11, tx, ty;
		bool translationOnly;
	};

	// What beginTarget saves for endTarget.
	struct Target
	{
		Mat4 projection;
		Vec2i size;
		bool scissorOn;
		Vec2i scissorPosition;
		Vec2i scissorSize;
	};

	void submit(const RenderState& s, const float* p_x, const float* p_y,
				const float* p_u, const float* p_v, const Vec4f* p_colors);
	void submitFlat(const float* p_x, const float* p_y, const Vec4f& color);
	// A quad already in the target's pixels.
	void pushQuad(const RenderState& s, const Vec2f* p_positions, const Vec2f* p_uvs, const Vec4f* p_colors);
	void requireState(const RenderState& s);
	void applyRareState();
	void applyState();
	void draw();
	void checkRecord();
	void bakePoint(float x, float y, float* p_outX, float* p_outY) const;
	RenderState flatState() const;
	void bindReal(uint id);
	void applyBlendMode(BlendMode blend);
	void applyScissor();

	// What is queued, and what it was queued against.
	std::vector<Vertex> stream;
	RenderState streamState;

	// The state the next quad is drawn under.
	RenderState current;

	std::vector<Transform> transforms;

	// The rare state as the scopes left it.
	bool colorMask[4];
	int stencilWriteRef;   // -1 for none
	int stencilTestRef;    // -1 for none
	bool discardTransparent;
	bool scissorOn;
	Vec2i scissorPosition;
	Vec2i scissorSize;

	Vec2i targetSize;
	std::vector<Target> targets;

	// GL objects, and what GL is believed to hold. glKnown covers the program,
	// its buffers and arrays; the rest is known on its own.
	uint program;
	uint vertexBuffer;
	uint indexBuffer;
	uint whiteTexture;
	int uniformProjection;
	int uniformTexture;
	int uniformDiscard;
	bool glKnown;
	bool glRareKnown;
	uint glBinding;
	bool glBindingKnown;
	BlendMode glBlend;
	bool glBlendKnown;
	bool glWriteMask[4];
	int glStencilWriteRef;
	int glStencilTestRef;
	bool glScissorOn;
	Vec2i glScissorPosition;
	Vec2i glScissorSize;
	bool glDiscard;
	Mat4 projection;
	bool projectionDirty;

	Stats counters;
	bool flushAll;
};

#endif
