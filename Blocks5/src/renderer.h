#ifndef _RENDERER_H
#define _RENDERER_H

/*** The one path every quad takes to the screen ***/

// Everything the game draws goes through here: a quad is baked - transform,
// uv, colour - into a stream of 32-byte vertices and drawn with the next
// flush, which happens only where the state a quad is drawn under changes
// (the texture or the blend, see renderstate.h), where a scope begins or
// ends, where the stream is full, or where something raw needs the screen
// in order. RENDERER-REDESIGN.md is the design, .claude/rules/rendering.md
// what the two modes of the interim are; renderer.cpp carries the reasons.

#include "renderstate.h"

// The stream vertex: 32 bytes, 2D, transform applied and uv normalised.
struct Vertex
{
	Vec2f position;
	Vec2f uv;
	Vec4f color;
};

// The cached vertex: position and uv in texels, no colour, 16 bytes, so one
// built array can be drawn several times under another colour and offset.
struct QuadVertex
{
	QuadVertex(double px, double py, double u, double v)
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
		FR_EXPLICIT,  // flush() from outside, before a copy, a clear or a delete
		FR_FRAME,     // the end of the frame
		FR_DIRECT,    // a draw call inside a DirectGL bracket, or a bracket opening
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

	// Per frame: the frame buffer's 640x480 projection, the stack on the
	// identity.
	void frameBegin();
	void frameEnd();

	// Another projection for the length of a render-to-texture; flushes.
	void setProjection(const Mat4& matrix);
	const Mat4& getProjection() const { return projection; }

	// --- the state a quad is drawn under ---------------------------------
	//
	// "Texturing off" is the white texel; the bound texture is remembered
	// across it. setTexture binds for real whatever the mode, so that a raw
	// upload or copy right after it lands in that texture.
	void setTexture(const TextureRef& texture);
	void setTexturing(bool on);
	void setBlend(BlendMode blend);
	const RenderState& state() const { return current; }
	const TextureRef& boundTexture() const { return bound; }
	bool texturing() const { return texturingOn; }

	// glDeleteTextures for one, after a flush.
	void deleteTexture(uint id);

	// --- the transform, baked on the CPU -----------------------------------
	//
	// A 2D affine stack, applied at submission, so a change of it never
	// breaks a batch. Inside a DirectGL it moves GL's modelview as well.
	void push();
	void pop();
	void translate(double x, double y);
	void scale(double x, double y);
	void rotate(double degrees);
	void loadIdentity();

	// --- drawing -------------------------------------------------------------
	//
	// Positions in the caller's space, uv in texels of the state's texture,
	// colours as given (the program clamps them to 0..1).

	// A sprite as Engine::renderSprite defines it, under the current state.
	void sprite(const Vec2d& position, const Vec2i& halfSize, const Vec2i& otherHalf,
				int u0, int u1, int v0, int v1,
				const Vec4d& color, double rotation, double scaling);

	// A built array of quads under one colour: the tile grid, the font.
	void quads(const RenderState& s, const QuadVertex* p_vertices, uint count, const Vec4f& color);

	// The same from positions alone, flat: the font's keycap frames.
	void quads(const Vec2f* p_positions, uint count, const Vec4f& color);

	// Quads with a colour per vertex: the particle systems.
	void quads(const RenderState& s, const Vertex* p_vertices, uint count);

	// One quad from four corners in order, one colour or a colour a corner.
	void quad(const RenderState& s, const Vec2f* p_corners, const Vec2f* p_uvs, const Vec4f& color);
	void quad(const RenderState& s, const Vec2f* p_corners, const Vec2f* p_uvs, const Vec4f* p_colors);

	// Flat geometry, all under the current blend; a point is the disc of
	// the built-in texture.
	void rect(const Vec2f& min, const Vec2f& max, const Vec4f& color);
	void rectOutline(const Vec2f& min, const Vec2f& max, float width, const Vec4f& color);
	void line(const Vec2f& a, const Vec2f& b, float width, const Vec4f& color);
	void polyline(const std::vector<Vec2f>& points, float width, const Vec4f& color, bool closed = false);
	void point(const Vec2f& p, float size, const Vec4f& color);

	// Put up everything queued, under the state it was queued against.
	void flush(FlushReason reason = FR_EXPLICIT);

	// Clears flush first, because they replace what was drawn.
	void clear(const Vec4f& color);
	void clearStencil();

	// --- the rare state, as scopes ------------------------------------------
	//
	// Each flushes at both ends and puts the previous value back. Drawing
	// inside one is ordinary batched drawing; none may be open where a
	// DirectGL begins.

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

	// Raw GL for the length of a bracket: the constructor flushes and sets
	// the fixed function up, the destructor makes the renderer forget what
	// GL holds. Nests; only the outermost does anything.
	class DirectGL
	{
	public:
		DirectGL();
		~DirectGL();
	};

	// The other way round: batched drawing inside a DirectGL, starting from
	// GL's modelview as the raw code left it. Nothing where none is open.
	class Batched
	{
	public:
		Batched();
		~Batched();
	private:
		int suspended;
	};

	bool inDirectGL() const { return directDepth > 0; }

	// Forget what GL is holding, and nothing else (presentFrame).
	void invalidate();

	// --- measuring ----------------------------------------------------------
	const Stats& stats() const { return counters; }
	void resetStats();

	// -flushall: a flush after every quad.
	void setFlushAll(bool on) { flushAll = on; }
	bool isFlushAll() const { return flushAll; }

private:
	Renderer();
	~Renderer();

	// x' = m00 * x + m01 * y + tx; y' = m10 * x + m11 * y + ty. Float, as
	// GL's matrix stack was.
	struct Transform
	{
		float m00, m01, m10, m11, tx, ty;
		bool translationOnly;
	};

	void submit(const RenderState& s, const double* p_x, const double* p_y,
				const float* p_u, const float* p_v, const Vec4f* p_colors);
	void submitFlat(const double* p_x, const double* p_y, const Vec4f& color);
	// The end of every public draw call: inside a DirectGL it goes up at once.
	void endCall();
	void requireState(const RenderState& s);
	void applyState();
	void draw();
	void checkRecord();
	void bakePoint(double x, double y, float* p_outX, float* p_outY) const;
	RenderState flatState() const;
	// The GL calls behind the state, issued only where GL does not hold it.
	void bindReal(uint id);
	void applyTexelScale(const Vec2f& scale);
	void applyTexturing(bool on);
	void applyBlendMode(BlendMode blend);
	// The fixed-function pipeline as raw code expects it: DirectGL's entry
	// and Batched's exit.
	void enterDirect();

	// What is queued, and what it was queued against.
	std::vector<Vertex> stream;
	RenderState streamState;

	// The state the next quad is drawn under.
	RenderState current;
	TextureRef bound;
	bool texturingOn;

	std::vector<Transform> transforms;
	// GL's modelview, read at the first draw of a call inside a DirectGL.
	bool directMatrixKnown;
	float directMatrix[16];

	// The rare state as the scopes left it.
	bool colorMask[4];
	int stencilWriteRef;   // -1 for none
	int stencilTestRef;    // -1 for none
	bool discardTransparent;
	int scopeDepth;
	int directDepth;

	// GL objects, and what GL is believed to hold. glKnown covers the program,
	// its buffers and arrays, and the rare state; the binding, the texel
	// scale, the texturing enable and the blend are known on their own, since
	// every door to them comes through here, and so survive a raw section.
	uint program;
	uint vertexBuffer;
	uint indexBuffer;
	uint whiteTexture;
	int uniformProjection;
	int uniformTexture;
	int uniformDiscard;
	bool glKnown;
	uint glBinding;
	bool glBindingKnown;
	Vec2f glTexelScale;
	bool glScaleKnown;
	int glTexturing;       // -1 unknown, else 0 or 1
	BlendMode glBlend;
	bool glBlendKnown;
	bool glWriteMask[4];
	int glStencilWriteRef;
	int glStencilTestRef;
	bool glDiscard;
	Mat4 projection;
	bool projectionDirty;

	Stats counters;
	bool flushAll;
};

#endif
