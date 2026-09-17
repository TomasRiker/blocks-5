#include "pch.h"
#include "renderer.h"
#include "glextensions.h"
#include "fatalerror.h"

// renderer.cpp - what renderer.h promises. The GL side is small on purpose:
// one program, one texture of its own, two buffers, and a flush that applies
// only what changed. Everything else is arithmetic on the CPU.

namespace
{
	// The one shape a batch takes on every platform: GL_TRIANGLES out of a
	// static index buffer, six indices a quad, uint16 - which is where the
	// ceiling comes from: 65536 vertices are 16384 quads.
	const uint MAX_QUADS = 16384;

	// The built-in texture: a white block in the left half of its top row of
	// 16x16 cells and a soft disc in the right one. A flat draw samples the
	// centre of a texel inside the block, so that linear filtering has one
	// texel to weigh and the sample is exactly white; a point stretches the
	// disc over its own size.
	const int BUILTIN_SIZE = 32;
	const float BLOCK_U = 8.5f, BLOCK_V = 8.5f;
	const float DISC_U0 = 16.0f, DISC_V0 = 0.0f, DISC_U1 = 32.0f, DISC_V1 = 16.0f;

	// The eight blend functions, in the order of BlendMode.
	const GLenum BLEND_FACTORS[BM_COUNT][4] =
	{
		{GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE},                 // BM_NORMAL
		{GL_SRC_ALPHA, GL_ONE, GL_ONE, GL_ONE},                                 // BM_ADDITIVE
		{GL_DST_COLOR, GL_ZERO, GL_ONE, GL_ONE},                                // BM_MULTIPLY
		{GL_ZERO, GL_ZERO, GL_ZERO, GL_ZERO},                                   // BM_ZERO
		{GL_ONE, GL_ONE, GL_ONE, GL_ONE},                                       // BM_ADD_ALL
		{GL_ONE_MINUS_DST_ALPHA, GL_DST_ALPHA, GL_ONE, GL_ONE},                 // BM_DARKEN_UNLIT
		{GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA}, // BM_BAKE
		{GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA}        // BM_PREMULTIPLIED
	};

	// No #version, as in upscaler.cpp: 110 on the desktop, 100 in the browser,
	// and this compiles as both. a_position is a vec4 fed with two components,
	// which GL completes to (x, y, 0, 1) - the one program serves a 3D vertex
	// with a stride switch. The clamp is the desktop's fixed-function clamp of
	// a vertex colour, now on every platform: the game hands colours above 1
	// on purpose and relied on it.
	const char* const p_vertexShader =
		"attribute vec4 a_position;\n"
		"attribute vec2 a_uv;\n"
		"attribute vec4 a_color;\n"
		"uniform mat4 u_projection;\n"
		"varying vec2 v_uv;\n"
		"varying vec4 v_color;\n"
		"void main()\n"
		"{\n"
		"    gl_Position = u_projection * a_position;\n"
		"    v_uv = a_uv;\n"
		"    v_color = clamp(a_color, 0.0, 1.0);\n"
		"}\n";

	// highp where the browser has it: the uv of a scrolling texture is
	// what mediump quantises on a phone (.claude/rules/rendering.md).
	const char* const p_fragmentShader =
		"#ifdef GL_ES\n"
		"#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
		"precision highp float;\n"
		"#else\n"
		"precision mediump float;\n"
		"#endif\n"
		"#endif\n"
		"uniform sampler2D u_texture;\n"
		"uniform int u_discard;\n"
		"varying vec2 v_uv;\n"
		"varying vec4 v_color;\n"
		"void main()\n"
		"{\n"
		"    vec4 color = texture2D(u_texture, v_uv) * v_color;\n"
		"    if(u_discard != 0 && color.a <= 0.0) discard;\n"
		"    gl_FragColor = color;\n"
		"}\n";

	uint compileStage(GLenum type, const char* p_source, const char* p_what, std::string* p_log)
	{
		const uint shader = glExtCreateShader(type);
		if(!shader) return 0;
		glExtShaderSource(shader, 1, &p_source, 0);
		glExtCompileShader(shader);
		GLint ok = 0;
		glExtGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
		if(!ok)
		{
			char log[1024] = "";
			glExtGetShaderInfoLog(shader, sizeof(log) - 1, 0, log);
			*p_log = std::string(p_what) + " shader: " + log;
			glExtDeleteShader(shader);
			return 0;
		}
		return shader;
	}

	typedef void (APIENTRY* BlendFuncSeparateProc)(GLenum, GLenum, GLenum, GLenum);
	BlendFuncSeparateProc p_blendFuncSeparate = 0;

	void applyBlend(BlendMode mode)
	{
		const GLenum* f = BLEND_FACTORS[mode];
		p_blendFuncSeparate(f[0], f[1], f[2], f[3]);
	}

	void applyStencil(int writeRef, int testRef)
	{
		if(writeRef >= 0)
		{
			glEnable(GL_STENCIL_TEST);
			glStencilFunc(GL_ALWAYS, writeRef, 0xFF);
			glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
		}
		else if(testRef >= 0)
		{
			glEnable(GL_STENCIL_TEST);
			glStencilFunc(GL_EQUAL, testRef, 0xFF);
			glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
		}
		else glDisable(GL_STENCIL_TEST);
	}

	// The pixels a one-pixel GL line lit along its length, measured on
	// llvmpipe: a pixel whose centre lies in the half-open span from the
	// start point (inclusive) to the end point (exclusive) in the direction
	// of travel - so a line on whole coordinates covers [start, end)
	// whichever way it runs, and one through pixel centres lights the pixel
	// it starts in and not the one it ends in. Returns the [first, last)
	// pixel span.
	void lineSpan(float start, float end, float* p_first, float* p_last)
	{
		if(start <= end)
		{
			*p_first = ceilf(start - 0.5f);
			*p_last = ceilf(end - 0.5f);
		}
		else
		{
			*p_first = floorf(end - 0.5f) + 1.0f;
			*p_last = floorf(start - 0.5f) + 1.0f;
		}
	}
}

Renderer::Renderer()
{
	colorMask[0] = colorMask[1] = colorMask[2] = colorMask[3] = true;
	stencilWriteRef = stencilTestRef = -1;
	discardTransparent = false;
	scissorOn = false;
	scissorPosition = scissorSize = Vec2i(0, 0);
	targetSize = Vec2i(0, 0);
	program = vertexBuffer = indexBuffer = whiteTexture = 0;
	uniformProjection = uniformTexture = uniformDiscard = -1;
	glKnown = false;
	glRareKnown = false;
	glBinding = 0;
	glBindingKnown = false;
	glBlend = BM_NORMAL;
	glBlendKnown = false;
	glWriteMask[0] = glWriteMask[1] = glWriteMask[2] = glWriteMask[3] = true;
	glStencilWriteRef = glStencilTestRef = -1;
	glScissorOn = false;
	glScissorPosition = glScissorSize = Vec2i(0, 0);
	glDiscard = false;
	projection = Mat4::identity();
	projectionDirty = true;
	flushAll = false;
	resetStats();

	Transform identity;
	identity.m00 = identity.m11 = 1.0f;
	identity.m01 = identity.m10 = identity.tx = identity.ty = 0.0f;
	identity.translationOnly = true;
	transforms.push_back(identity);
}

Renderer::~Renderer()
{
}

void Renderer::init()
{
#ifdef __EMSCRIPTEN__
	p_blendFuncSeparate = reinterpret_cast<BlendFuncSeparateProc>(&glBlendFuncSeparate);
#else
	p_blendFuncSeparate = reinterpret_cast<BlendFuncSeparateProc>(SDL_GL_GetProcAddress("glBlendFuncSeparate"));
	if(!p_blendFuncSeparate)
		p_blendFuncSeparate = reinterpret_cast<BlendFuncSeparateProc>(SDL_GL_GetProcAddress("glBlendFuncSeparateEXT"));
#endif
	if(!p_blendFuncSeparate)
	{
		fatalError("Blocks 5", "This OpenGL driver has no glBlendFuncSeparate, which OpenGL 1.4 "
				   "and every later version have. Please install the graphics driver.");
	}

	// The program.
	std::string log;
	const uint vs = compileStage(GL_VERTEX_SHADER, p_vertexShader, "vertex", &log);
	const uint fs = vs ? compileStage(GL_FRAGMENT_SHADER, p_fragmentShader, "fragment", &log) : 0;
	if(vs && fs)
	{
		program = glExtCreateProgram();
		glExtAttachShader(program, vs);
		glExtAttachShader(program, fs);
		glExtBindAttribLocation(program, 0, "a_position");
		glExtBindAttribLocation(program, 1, "a_uv");
		glExtBindAttribLocation(program, 2, "a_color");
		glExtLinkProgram(program);
		GLint ok = 0;
		glExtGetProgramiv(program, GL_LINK_STATUS, &ok);
		if(!ok)
		{
			char info[1024] = "";
			glExtGetProgramInfoLog(program, sizeof(info) - 1, 0, info);
			log = std::string("program: ") + info;
			glExtDeleteProgram(program);
			program = 0;
		}
	}
	if(vs) glExtDeleteShader(vs);
	if(fs) glExtDeleteShader(fs);
	if(!program)
	{
		const char* p_version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
		const char* p_renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
		fatalError("Blocks 5", "The renderer's shader program could not be built (" + log + ").\n\n"
				   "OpenGL reports version \"" + std::string(p_version ? p_version : "?") +
				   "\" on \"" + std::string(p_renderer ? p_renderer : "?") +
				   "\". Blocks 5 needs OpenGL 2.0; please install the graphics driver.");
	}
	uniformProjection = glExtGetUniformLocation(program, "u_projection");
	uniformTexture = glExtGetUniformLocation(program, "u_texture");
	uniformDiscard = glExtGetUniformLocation(program, "u_discard");

	// The two uniforms that never move again: the sampler is unit 0 and the
	// discard starts off. Uniforms live in the program, not in the context,
	// so nothing outside this file can change them.
	glExtUseProgram(program);
	glExtUniform1i(uniformTexture, 0);
	glExtUniform1i(uniformDiscard, 0);
	glExtUseProgram(0);
	glDiscard = false;

	// The built-in texture. Linear filtering and clamping: the block is
	// sampled at one point and the disc's own edge is what a point's edge
	// becomes, so neither may bleed into the other or wrap.
	{
		std::vector<uchar> pixels(BUILTIN_SIZE * BUILTIN_SIZE * 4, 0);
		for(int y = 0; y < 16; y++)
		{
			for(int x = 0; x < 16; x++)
			{
				uchar* p_block = &pixels[(y * BUILTIN_SIZE + x) * 4];
				p_block[0] = p_block[1] = p_block[2] = p_block[3] = 255;

				// Alpha 1 to a radius of 7, 0 from 8, a ramp between: one
				// texel of edge, which linear filtering spreads over the
				// size the point is drawn at. (x, y) is local to the cell.
				const float dx = x + 0.5f - 8.0f;
				const float dy = y + 0.5f - 8.0f;
				const float distance = sqrtf(dx * dx + dy * dy);
				float alpha = 8.0f - distance;
				if(alpha < 0.0f) alpha = 0.0f;
				if(alpha > 1.0f) alpha = 1.0f;
				uchar* p_disc = &pixels[(y * BUILTIN_SIZE + 16 + x) * 4];
				p_disc[0] = p_disc[1] = p_disc[2] = 255;
				p_disc[3] = static_cast<uchar>(alpha * 255.0f + 0.5f);
			}
		}
		glGenTextures(1, &whiteTexture);
		glBindTexture(GL_TEXTURE_2D, whiteTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, BUILTIN_SIZE, BUILTIN_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, &pixels[0]);
		glBindTexture(GL_TEXTURE_2D, current.texture.id);
		glBinding = current.texture.id;
		glBindingKnown = true;
	}

	// The buffers. The index buffer is built once: 0 1 2, 0 2 3 for every
	// quad, split along the diagonal from the first corner to the third.
	// Which diagonal shows wherever an attribute is not affine across the
	// quad - the lava's four alphas, the lightning's trapezoids - and Mesa
	// uses this one for a glBegin(GL_QUADS) quad and the other for a quad
	// out of an array. The one the immediate-mode quads had, because they
	// are the many; RENDERER-REDESIGN.md section 6 names the lightning as
	// the cost.
	glExtGenBuffers(1, &vertexBuffer);
	glExtGenBuffers(1, &indexBuffer);
	{
		std::vector<ushort> indices(MAX_QUADS * 6);
		for(uint q = 0; q < MAX_QUADS; q++)
		{
			const ushort base = static_cast<ushort>(q * 4);
			ushort* p = &indices[q * 6];
			p[0] = base; p[1] = base + 1; p[2] = base + 2;
			p[3] = base; p[4] = base + 2; p[5] = base + 3;
		}
		glExtBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
		glExtBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(ushort), &indices[0], GL_STATIC_DRAW);
		glExtBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}

	stream.reserve(4 * 4096);
	glKnown = false;
	printfLog("  Renderer: program %u, built-in texture %u, %u quads a draw.\n", program, whiteTexture, MAX_QUADS);
}

void Renderer::frameBegin(const Vec2i& size)
{
	targetSize = size;
	targets.clear();
	projection = Mat4::ortho(0.0f, static_cast<float>(size.x), static_cast<float>(size.y), 0.0f, -1.0f, 1.0f);
	projectionDirty = true;
	// Every frame starts on the identity: a push without its pop would
	// otherwise carry into the next frame, where nothing would explain it.
	transforms.resize(1);
	loadIdentity();
	scissorOn = false;
}

void Renderer::frameEnd()
{
	flush(FR_FRAME);
}

void Renderer::beginTarget(const Vec2i& size)
{
	flush(FR_EXPLICIT);
	Target target;
	target.projection = projection;
	target.size = targetSize;
	target.scissorOn = scissorOn;
	target.scissorPosition = scissorPosition;
	target.scissorSize = scissorSize;
	targets.push_back(target);

	// A scissor set for the frame is in the frame's pixels and would clip
	// the texture here, the clear included.
	scissorOn = false;
	targetSize = size;
	projection = Mat4::ortho(0.0f, static_cast<float>(size.x), static_cast<float>(size.y), 0.0f, -1.0f, 1.0f);
	projectionDirty = true;
	// A bake starts from the texture's origin whichever transform the
	// caller stood under.
	transforms.push_back(transforms.back());
	loadIdentity();
}

void Renderer::endTarget()
{
	flush(FR_EXPLICIT);
	if(targets.empty()) return;
	const Target& target = targets.back();
	projection = target.projection;
	targetSize = target.size;
	scissorOn = target.scissorOn;
	scissorPosition = target.scissorPosition;
	scissorSize = target.scissorSize;
	targets.pop_back();
	projectionDirty = true;
	if(transforms.size() > 1) transforms.pop_back();
}

RenderState Renderer::flatState() const
{
	return RenderState(TextureRef(0, Vec2f(1.0f / BUILTIN_SIZE, 1.0f / BUILTIN_SIZE)), current.blend);
}

// --- the GL calls behind the state -------------------------------------------

void Renderer::bindReal(uint id)
{
	if(glBindingKnown && glBinding == id) return;
	glBindTexture(GL_TEXTURE_2D, id);
	glBinding = id;
	glBindingKnown = true;
}

void Renderer::applyBlendMode(BlendMode blend)
{
	if(glBlendKnown && glBlend == blend) return;
	// The enable is this file's state as much as the function is: a
	// DirectGL bracket that switched it off - the present does - drops the
	// record, and the next flush puts both back.
	if(!glBlendKnown) glEnable(GL_BLEND);
	applyBlend(blend);
	glBlend = blend;
	glBlendKnown = true;
}

void Renderer::applyScissor()
{
	if(scissorOn)
	{
		// GL's box has its origin at the bottom left of the target.
		glEnable(GL_SCISSOR_TEST);
		glScissor(scissorPosition.x, targetSize.y - scissorPosition.y - scissorSize.y,
				  scissorSize.x, scissorSize.y);
	}
	else glDisable(GL_SCISSOR_TEST);
}

// --- state -----------------------------------------------------------------

void Renderer::setTexture(const TextureRef& texture)
{
	current.texture = texture;
	bindReal(texture.id);
}

void Renderer::setBlend(BlendMode blend)
{
	current.blend = blend;
}

void Renderer::deleteTexture(uint id)
{
	flush(FR_EXPLICIT);
	glDeleteTextures(1, &id);
	if(current.texture.id == id) current.texture = TextureRef();
	// GL unbinds a deleted texture itself.
	if(glBindingKnown && glBinding == id) glBinding = 0;
}

// --- the transform ---------------------------------------------------------

void Renderer::push()
{
	transforms.push_back(transforms.back());
}

void Renderer::pop()
{
	if(transforms.size() > 1) transforms.pop_back();
}

void Renderer::translate(double x, double y)
{
	// In float, and in Mesa's order of operations for glTranslated - the
	// products first, the old translation last - so that the same corner
	// then lands in the same place it did under the matrix stack.
	Transform& t = transforms.back();
	const float fx = static_cast<float>(x), fy = static_cast<float>(y);
	t.tx = t.m00 * fx + t.m01 * fy + t.tx;
	t.ty = t.m10 * fx + t.m11 * fy + t.ty;
}

void Renderer::scale(double x, double y)
{
	Transform& t = transforms.back();
	const float fx = static_cast<float>(x), fy = static_cast<float>(y);
	t.m00 *= fx; t.m10 *= fx;
	t.m01 *= fy; t.m11 *= fy;
	t.translationOnly = false;
}

void Renderer::rotate(double degrees)
{
	float s, c;
	Mat4::rotationTerms(degrees, &s, &c);
	Transform& t = transforms.back();
	const float m00 = t.m00 * c + t.m01 * s;
	const float m01 = t.m00 * -s + t.m01 * c;
	const float m10 = t.m10 * c + t.m11 * s;
	const float m11 = t.m10 * -s + t.m11 * c;
	t.m00 = m00; t.m01 = m01; t.m10 = m10; t.m11 = m11;
	t.translationOnly = false;
}

void Renderer::loadIdentity()
{
	Transform& t = transforms.back();
	t.m00 = t.m11 = 1.0f;
	t.m01 = t.m10 = t.tx = t.ty = 0.0f;
	t.translationOnly = true;
}

void Renderer::bakePoint(double x, double y, float* p_outX, float* p_outY) const
{
	// The float matrix entries promote to double and the sum rounds once, to
	// float - the arithmetic the old sprite batch did with the matrix it read
	// back from GL, kept so that a baked corner is the same float.
	const Transform& t = transforms.back();
	if(t.translationOnly)
	{
		*p_outX = static_cast<float>(x + t.tx);
		*p_outY = static_cast<float>(y + t.ty);
	}
	else
	{
		*p_outX = static_cast<float>(t.m00 * x + t.m01 * y + t.tx);
		*p_outY = static_cast<float>(t.m10 * x + t.m11 * y + t.ty);
	}
}

// --- drawing -----------------------------------------------------------------

void Renderer::requireState(const RenderState& s)
{
	if(stream.empty()) { streamState = s; return; }
	if(streamState.texture.id != s.texture.id) flush(FR_TEXTURE);
	else if(streamState.blend != s.blend) flush(FR_BLEND);
	else if(stream.size() >= 4 * MAX_QUADS) flush(FR_FULL);
	if(stream.empty()) streamState = s;
}

void Renderer::pushQuad(const RenderState& s, const Vec2f* p_positions, const Vec2f* p_uvs, const Vec4f* p_colors)
{
	requireState(s);
	for(int i = 0; i < 4; i++)
	{
		Vertex vertex;
		vertex.position = p_positions[i];
		vertex.uv = Vec2f(p_uvs[i].x * s.texture.texelScale.x, p_uvs[i].y * s.texture.texelScale.y);
		vertex.color = p_colors[i];
		stream.push_back(vertex);
	}
	if(flushAll) flush(FR_EXPLICIT);
}

void Renderer::submit(const RenderState& s, const double* p_x, const double* p_y,
					  const float* p_u, const float* p_v, const Vec4f* p_colors)
{
	Vec2f positions[4], uvs[4];
	for(int i = 0; i < 4; i++)
	{
		bakePoint(p_x[i], p_y[i], &positions[i].x, &positions[i].y);
		uvs[i] = Vec2f(p_u[i], p_v[i]);
	}
	pushQuad(s, positions, uvs, p_colors);
}

void Renderer::submitFlat(const double* p_x, const double* p_y, const Vec4f& color)
{
	const float u[4] = {BLOCK_U, BLOCK_U, BLOCK_U, BLOCK_U};
	const float v[4] = {BLOCK_V, BLOCK_V, BLOCK_V, BLOCK_V};
	const Vec4f colors[4] = {color, color, color, color};
	submit(flatState(), p_x, p_y, u, v, colors);
}

void Renderer::sprite(const Vec2d& position, const Vec2i& halfSize, const Vec2i& otherHalf,
					  int u0, int u1, int v0, int v1,
					  const Vec4d& color, double rotation, double scaling)
{
	// The sprite's own transform in double, as the batch always did it:
	// rotate, then scale, then translate to the centre. Mirroring is already
	// in the texture coordinates.
	double c = scaling;
	double s = 0.0;
	if(rotation != 0.0)
	{
		const double a = rotation * (3.1415926535897932384626433832795 / 180.0);
		c = scaling * cos(a);
		s = scaling * sin(a);
	}

	const double tx = position.x + halfSize.x;
	const double ty = position.y + halfSize.y;
	// Widened one at a time and not inside the braces: a braced initializer
	// list forbids a narrowing conversion, and clang says so where gcc does not.
	const double left = -halfSize.x, right = otherHalf.x;
	const double top = -halfSize.y, bottom = otherHalf.y;
	const double lx[4] = {left, right, right, left};
	const double ly[4] = {top, top, bottom, bottom};
	const float u[4] = {static_cast<float>(u0), static_cast<float>(u1), static_cast<float>(u1), static_cast<float>(u0)};
	const float v[4] = {static_cast<float>(v0), static_cast<float>(v0), static_cast<float>(v1), static_cast<float>(v1)};

	double x[4], y[4];
	for(int i = 0; i < 4; i++)
	{
		x[i] = tx + c * lx[i] - s * ly[i];
		y[i] = ty + s * lx[i] + c * ly[i];
	}
	const Vec4f col(static_cast<float>(color.r), static_cast<float>(color.g),
					static_cast<float>(color.b), static_cast<float>(color.a));
	const Vec4f colors[4] = {col, col, col, col};
	submit(current, x, y, u, v, colors);
}

void Renderer::quads(const RenderState& s, const QuadVertex* p_vertices, uint count, const Vec4f& color)
{
	const Vec4f colors[4] = {color, color, color, color};
	for(uint q = 0; q + 4 <= count; q += 4)
	{
		double x[4], y[4];
		float u[4], v[4];
		for(int i = 0; i < 4; i++)
		{
			x[i] = p_vertices[q + i].position.x;
			y[i] = p_vertices[q + i].position.y;
			u[i] = p_vertices[q + i].uv.x;
			v[i] = p_vertices[q + i].uv.y;
		}
		submit(s, x, y, u, v, colors);
	}
}

void Renderer::quads(const RenderState& s, const Vertex* p_vertices, uint count)
{
	for(uint q = 0; q + 4 <= count; q += 4)
	{
		double x[4], y[4];
		float u[4], v[4];
		Vec4f colors[4];
		for(int i = 0; i < 4; i++)
		{
			x[i] = p_vertices[q + i].position.x;
			y[i] = p_vertices[q + i].position.y;
			u[i] = p_vertices[q + i].uv.x;
			v[i] = p_vertices[q + i].uv.y;
			colors[i] = p_vertices[q + i].color;
		}
		submit(s, x, y, u, v, colors);
	}
}

void Renderer::quad(const RenderState& s, const Vec2f* p_corners, const Vec2f* p_uvs, const Vec4f& color)
{
	const Vec4f colors[4] = {color, color, color, color};
	quad(s, p_corners, p_uvs, colors);
}

void Renderer::quad(const RenderState& s, const Vec2f* p_corners, const Vec2f* p_uvs, const Vec4f* p_colors)
{
	double x[4], y[4];
	float u[4], v[4];
	for(int i = 0; i < 4; i++)
	{
		x[i] = p_corners[i].x; y[i] = p_corners[i].y;
		u[i] = p_uvs[i].x; v[i] = p_uvs[i].y;
	}
	submit(s, x, y, u, v, p_colors);
}

void Renderer::scrolledQuad(uint textureId, const Mat4& textureMatrix, const Vec2f* p_corners, const Vec2f* p_uvs, const Vec4f& color)
{
	Vec2f uvs[4];
	for(int i = 0; i < 4; i++) uvs[i] = textureMatrix.transformPoint2D(p_uvs[i]);
	quad(RenderState(TextureRef(textureId, Vec2f(1.0f, 1.0f)), current.blend), p_corners, uvs, color);
}

void Renderer::quad(const Vec2f* p_corners, const Vec4f* p_colors)
{
	double x[4], y[4];
	const float u[4] = {BLOCK_U, BLOCK_U, BLOCK_U, BLOCK_U};
	const float v[4] = {BLOCK_V, BLOCK_V, BLOCK_V, BLOCK_V};
	for(int i = 0; i < 4; i++)
	{
		x[i] = p_corners[i].x; y[i] = p_corners[i].y;
	}
	submit(flatState(), x, y, u, v, p_colors);
}

void Renderer::triangles(const RenderState& s, const Vertex* p_vertices, uint count)
{
	// A triangle is a quad whose fourth corner repeats its third: the
	// second triangle of the split has no area and no pixel, and the first
	// keeps the three vertices in the order given.
	for(uint t = 0; t + 3 <= count; t += 3)
	{
		double x[4], y[4];
		float u[4], v[4];
		Vec4f colors[4];
		for(int i = 0; i < 4; i++)
		{
			const Vertex& vertex = p_vertices[t + (i < 3 ? i : 2)];
			x[i] = vertex.position.x;
			y[i] = vertex.position.y;
			u[i] = vertex.uv.x;
			v[i] = vertex.uv.y;
			colors[i] = vertex.color;
		}
		submit(s, x, y, u, v, colors);
	}
}

void Renderer::triangles(const Vec2f* p_positions, const Vec4f* p_colors, uint count)
{
	const float u[4] = {BLOCK_U, BLOCK_U, BLOCK_U, BLOCK_U};
	const float v[4] = {BLOCK_V, BLOCK_V, BLOCK_V, BLOCK_V};
	for(uint t = 0; t + 3 <= count; t += 3)
	{
		double x[4], y[4];
		Vec4f colors[4];
		for(int i = 0; i < 4; i++)
		{
			const uint k = t + (i < 3 ? i : 2);
			x[i] = p_positions[k].x;
			y[i] = p_positions[k].y;
			colors[i] = p_colors[k];
		}
		submit(flatState(), x, y, u, v, colors);
	}
}

void Renderer::quads3D(const RenderState& s, const Mat4& transform, const Vertex3* p_vertices, uint count, bool cullBackFaces)
{
	// One draw of its own: the batch goes up first, then the 3D layout and
	// the caller's matrix take the program over for this draw, and the
	// next flush puts the 2D layout and the projection back - glKnown and
	// projectionDirty see to that.
	flush(FR_EXPLICIT);
	applyRareState();
	glExtUseProgram(program);
	glExtBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
	glExtBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
	glExtVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3), reinterpret_cast<const void*>(0));
	glExtVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex3), reinterpret_cast<const void*>(3 * sizeof(float)));
	glExtVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3), reinterpret_cast<const void*>(5 * sizeof(float)));
	glExtEnableVertexAttribArray(0);
	glExtEnableVertexAttribArray(1);
	glExtEnableVertexAttribArray(2);
	glExtUniformMatrix4fv(uniformProjection, 1, GL_FALSE, transform.m);
	projectionDirty = true;
	if(glDiscard != discardTransparent)
	{
		glExtUniform1i(uniformDiscard, discardTransparent ? 1 : 0);
		glDiscard = discardTransparent;
	}
	bindReal(s.texture.id ? s.texture.id : whiteTexture);
	applyBlendMode(s.blend);
	if(cullBackFaces) glEnable(GL_CULL_FACE);

	// The uv normalised on the way in, as for a 2D quad.
	std::vector<Vertex3> baked;
	for(uint q = 0; q + 4 <= count; q += MAX_QUADS * 4)
	{
		const uint n = min(count - q, MAX_QUADS * 4) / 4 * 4;
		baked.assign(p_vertices + q, p_vertices + q + n);
		for(uint i = 0; i < n; i++)
		{
			baked[i].uv.x *= s.texture.texelScale.x;
			baked[i].uv.y *= s.texture.texelScale.y;
		}
		glExtBufferData(GL_ARRAY_BUFFER, n * sizeof(Vertex3), &baked[0], GL_STREAM_DRAW);
		glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(n / 4 * 6), GL_UNSIGNED_SHORT, reinterpret_cast<const void*>(0));
		counters.draws++;
		counters.drawsByReason[FR_EXPLICIT]++;
		counters.quads += n / 4;
	}

	if(cullBackFaces) glDisable(GL_CULL_FACE);
	bindReal(current.texture.id);
	glKnown = false;
}

void Renderer::rect(const Vec2f& min, const Vec2f& max, const Vec4f& color)
{
	const double x[4] = {min.x, max.x, max.x, min.x};
	const double y[4] = {min.y, min.y, max.y, max.y};
	submitFlat(x, y, color);
}

void Renderer::quads(const Vec2f* p_positions, uint count, const Vec4f& color)
{
	for(uint q = 0; q + 4 <= count; q += 4)
	{
		double x[4], y[4];
		for(int i = 0; i < 4; i++)
		{
			x[i] = p_positions[q + i].x;
			y[i] = p_positions[q + i].y;
		}
		submitFlat(x, y, color);
	}
}

void Renderer::rectOutline(const Vec2f& min, const Vec2f& max, float width, const Vec4f& color)
{
	// Four bars that meet at the corners, the outline a line loop drew.
	const double bars[4][4] =
	{
		{min.x, min.y, max.x, min.y + width},
		{min.x, max.y - width, max.x, max.y},
		{min.x, min.y + width, min.x + width, max.y - width},
		{max.x - width, min.y + width, max.x, max.y - width}
	};
	for(int b = 0; b < 4; b++)
	{
		const double x[4] = {bars[b][0], bars[b][2], bars[b][2], bars[b][0]};
		const double y[4] = {bars[b][1], bars[b][1], bars[b][3], bars[b][3]};
		submitFlat(x, y, color);
	}
}

void Renderer::line(const Vec2f& a, const Vec2f& b, float width, const Vec4f& color)
{
	std::vector<Vec2f> points;
	points.push_back(a);
	points.push_back(b);
	polyline(points, width, color, false);
}

void Renderer::polyline(const std::vector<Vec2f>& points, float width, const Vec4f& color, bool closed)
{
	// One quad per segment, half the width to either side, and at a turn of
	// up to ninety degrees a wedge on the outside of the corner - a quad with
	// two corners on the point, which is a triangle - between the two
	// segments' ends, so a beam has no notch where it bends. A closed loop
	// gets its last segment and no wedge at the seam.
	const size_t n = points.size();
	if(n < 2) return;
	const size_t segments = closed ? n : n - 1;

	Vec2f prevDir(0.0f, 0.0f), prevUp(0.0f, 0.0f);
	bool havePrev = false;
	for(size_t i = 0; i < segments; i++)
	{
		const Vec2f& a = points[i];
		const Vec2f& b = points[(i + 1) % n];
		Vec2f dir(b.x - a.x, b.y - a.y);
		const float length = sqrtf(dir.x * dir.x + dir.y * dir.y);
		if(length <= 0.0f) continue;
		dir.x /= length; dir.y /= length;
		const Vec2f up(dir.y * 0.5f * width, -dir.x * 0.5f * width);

		if(havePrev && prevDir.x * dir.y - prevDir.y * dir.x >= 0.0f)
		{
			const float dot = prevUp.x * dir.y - prevUp.y * dir.x;
			if(dot > 0.0f)
			{
				const double x[4] = {a.x, a.x, a.x - up.x, a.x - prevUp.x};
				const double y[4] = {a.y, a.y, a.y - up.y, a.y - prevUp.y};
				submitFlat(x, y, color);
			}
			else if(dot < 0.0f)
			{
				const double x[4] = {a.x, a.x, a.x + prevUp.x, a.x + up.x};
				const double y[4] = {a.y, a.y, a.y + prevUp.y, a.y + up.y};
				submitFlat(x, y, color);
			}
		}

		const double x[4] = {a.x + up.x, b.x + up.x, b.x - up.x, a.x - up.x};
		const double y[4] = {a.y + up.y, b.y + up.y, b.y - up.y, a.y - up.y};
		submitFlat(x, y, color);

		prevDir = dir;
		prevUp = up;
		havePrev = true;
	}
}

void Renderer::point(const Vec2f& p, float size, const Vec4f& color)
{
	const float half = size * 0.5f;
	const double x[4] = {p.x - half, p.x + half, p.x + half, p.x - half};
	const double y[4] = {p.y - half, p.y - half, p.y + half, p.y + half};
	const float u[4] = {DISC_U0, DISC_U1, DISC_U1, DISC_U0};
	const float v[4] = {DISC_V0, DISC_V0, DISC_V1, DISC_V1};
	const Vec4f colors[4] = {color, color, color, color};
	submit(flatState(), x, y, u, v, colors);
}

void Renderer::hairline(const Vec2f& a, const Vec2f& b, const Vec4f& colorA, const Vec4f& colorB)
{
	// Baked first, so that the rule below is applied where the pixels are.
	// Across the line a pixel is the one containing the coordinate, a
	// boundary going to the lower side in GL's own coordinates - the column
	// to the left, the row below on the screen - which is llvmpipe's
	// tie-break, measured; along it, lineSpan. A line that is not
	// axis-aligned is the quad polyline would build for it.
	Vec2f p, q;
	bakePoint(a.x, a.y, &p.x, &p.y);
	bakePoint(b.x, b.y, &q.x, &q.y);
	Vec2f corners[4];
	Vec4f colors[4];
	if(p.y == q.y)
	{
		const float row = floorf(p.y);
		float first, last;
		lineSpan(p.x, q.x, &first, &last);
		corners[0] = Vec2f(first, row);
		corners[1] = Vec2f(last, row);
		corners[2] = Vec2f(last, row + 1.0f);
		corners[3] = Vec2f(first, row + 1.0f);
		const bool aFirst = p.x <= q.x;
		colors[0] = colors[3] = aFirst ? colorA : colorB;
		colors[1] = colors[2] = aFirst ? colorB : colorA;
	}
	else if(p.x == q.x)
	{
		const float column = ceilf(p.x) - 1.0f;
		float first, last;
		lineSpan(p.y, q.y, &first, &last);
		corners[0] = Vec2f(column, first);
		corners[1] = Vec2f(column + 1.0f, first);
		corners[2] = Vec2f(column + 1.0f, last);
		corners[3] = Vec2f(column, last);
		const bool aFirst = p.y <= q.y;
		colors[0] = colors[1] = aFirst ? colorA : colorB;
		colors[2] = colors[3] = aFirst ? colorB : colorA;
	}
	else
	{
		Vec2f dir(q.x - p.x, q.y - p.y);
		const float length = sqrtf(dir.x * dir.x + dir.y * dir.y);
		dir.x /= length; dir.y /= length;
		const Vec2f up(dir.y * 0.5f, -dir.x * 0.5f);
		corners[0] = Vec2f(p.x + up.x, p.y + up.y);
		corners[1] = Vec2f(q.x + up.x, q.y + up.y);
		corners[2] = Vec2f(q.x - up.x, q.y - up.y);
		corners[3] = Vec2f(p.x - up.x, p.y - up.y);
		colors[0] = colors[3] = colorA;
		colors[1] = colors[2] = colorB;
	}
	const Vec2f uvs[4] = {Vec2f(BLOCK_U, BLOCK_V), Vec2f(BLOCK_U, BLOCK_V), Vec2f(BLOCK_U, BLOCK_V), Vec2f(BLOCK_U, BLOCK_V)};
	pushQuad(flatState(), corners, uvs, colors);
}

void Renderer::hairlineRect(const Vec2f& min, const Vec2f& max, const Vec4f& color)
{
	// The four lines of a loop drawn clockwise from the top left, each on
	// its own: where two of them meet on the same pixel it is lit twice, as
	// GL lit it.
	const Vec2f topRight(max.x, min.y), bottomLeft(min.x, max.y);
	hairline(min, topRight, color);
	hairline(topRight, max, color);
	hairline(max, bottomLeft, color);
	hairline(bottomLeft, min, color);
}

void Renderer::dashes(const std::vector<Vec2f>& points, float width, const Vec4f& color,
					  float on, float off, float phase, bool closed)
{
	const size_t n = points.size();
	const float period = on + off;
	if(n < 2 || period <= 0.0f) return;
	const size_t segments = closed ? n : n - 1;

	// Where along the pattern the path stands; it runs on across corners.
	float travelled = phase;
	for(size_t i = 0; i < segments; i++)
	{
		const Vec2f& a = points[i];
		const Vec2f& b = points[(i + 1) % n];
		Vec2f dir(b.x - a.x, b.y - a.y);
		const float length = sqrtf(dir.x * dir.x + dir.y * dir.y);
		if(length <= 0.0f) continue;
		dir.x /= length; dir.y /= length;
		const Vec2f up(dir.y * 0.5f * width, -dir.x * 0.5f * width);

		float at = 0.0f;
		while(at < length)
		{
			float inPattern = fmodf(travelled + at, period);
			if(inPattern < 0.0f) inPattern += period;
			if(inPattern < on)
			{
				const float run = min(on - inPattern, length - at);
				const Vec2f from(a.x + dir.x * at, a.y + dir.y * at);
				const Vec2f to(a.x + dir.x * (at + run), a.y + dir.y * (at + run));
				const double x[4] = {from.x + up.x, to.x + up.x, to.x - up.x, from.x - up.x};
				const double y[4] = {from.y + up.y, to.y + up.y, to.y - up.y, from.y - up.y};
				submitFlat(x, y, color);
				at += run;
			}
			else at += period - inPattern;
		}
		travelled += length;
	}
}

// --- the flush ---------------------------------------------------------------

void Renderer::applyRareState()
{
	if(!glRareKnown || memcmp(glWriteMask, colorMask, sizeof(colorMask)))
	{
		glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
		memcpy(glWriteMask, colorMask, sizeof(colorMask));
	}
	if(!glRareKnown || glStencilWriteRef != stencilWriteRef || glStencilTestRef != stencilTestRef)
	{
		applyStencil(stencilWriteRef, stencilTestRef);
		glStencilWriteRef = stencilWriteRef;
		glStencilTestRef = stencilTestRef;
	}
	if(!glRareKnown || glScissorOn != scissorOn ||
	   (scissorOn && (glScissorPosition != scissorPosition || glScissorSize != scissorSize)))
	{
		applyScissor();
		glScissorOn = scissorOn;
		glScissorPosition = scissorPosition;
		glScissorSize = scissorSize;
	}
	glRareKnown = true;
}

void Renderer::applyState()
{
	const RenderState& s = streamState;
	if(!glKnown)
	{
		glExtUseProgram(program);
		glExtBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
		glExtBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
		glExtVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<const void*>(0));
		glExtVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<const void*>(2 * sizeof(float)));
		glExtVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<const void*>(4 * sizeof(float)));
		glExtEnableVertexAttribArray(0);
		glExtEnableVertexAttribArray(1);
		glExtEnableVertexAttribArray(2);
		glKnown = true;
	}
	applyRareState();

	// The uniforms live in the program and are set only where they moved.
	if(projectionDirty)
	{
		glExtUniformMatrix4fv(uniformProjection, 1, GL_FALSE, projection.m);
		projectionDirty = false;
	}
	if(glDiscard != discardTransparent)
	{
		glExtUniform1i(uniformDiscard, discardTransparent ? 1 : 0);
		glDiscard = discardTransparent;
	}

	bindReal(s.texture.id ? s.texture.id : whiteTexture);
	applyBlendMode(s.blend);
}

void Renderer::draw()
{
	const uint quadCount = static_cast<uint>(stream.size() / 4);
	glExtBufferData(GL_ARRAY_BUFFER, stream.size() * sizeof(Vertex), &stream[0], GL_STREAM_DRAW);
	glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(quadCount * 6), GL_UNSIGNED_SHORT, reinterpret_cast<const void*>(0));
}

void Renderer::flush(FlushReason reason)
{
	counters.flushes++;
	if(stream.empty()) return;

	applyState();
	draw();

	counters.draws++;
	counters.drawsByReason[reason]++;
	counters.quads += static_cast<uint>(stream.size() / 4);
	checkRecord();
	stream.clear();

	// The binding is the current texture again, which a raw upload or copy
	// after a setTexture relies on.
	bindReal(current.texture.id);
}

void Renderer::clear(const Vec4f& color)
{
	flush(FR_EXPLICIT);
	applyRareState();
	glClearColor(color.r, color.g, color.b, color.a);
	glClear(GL_COLOR_BUFFER_BIT);
}

void Renderer::clearStencil()
{
	flush(FR_EXPLICIT);
	applyRareState();
	glClearStencil(0);
	glClear(GL_STENCIL_BUFFER_BIT);
}

void Renderer::copyFrame(uint textureId, const Vec2i& destination, const Vec2i& size)
{
	flush(FR_EXPLICIT);
	bindReal(textureId);
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, destination.x, destination.y, 0, 0, size.x, size.y);
	bindReal(current.texture.id);
}

void Renderer::invalidate()
{
	glKnown = false;
	glRareKnown = false;
	glBindingKnown = false;
	glBlendKnown = false;
}

void Renderer::resetStats()
{
	memset(&counters, 0, sizeof(counters));
}

// --- the scopes --------------------------------------------------------------

Renderer::ColorMaskScope::ColorMaskScope(bool r, bool g, bool b, bool a)
{
	Renderer& renderer = Renderer::inst();
	renderer.flush(FR_SCOPE);
	memcpy(previous, renderer.colorMask, sizeof(previous));
	renderer.colorMask[0] = r; renderer.colorMask[1] = g;
	renderer.colorMask[2] = b; renderer.colorMask[3] = a;
}

Renderer::ColorMaskScope::~ColorMaskScope()
{
	Renderer& renderer = Renderer::inst();
	renderer.flush(FR_SCOPE);
	memcpy(renderer.colorMask, previous, sizeof(previous));
}

Renderer::StencilWriteScope::StencilWriteScope(int ref)
{
	Renderer& renderer = Renderer::inst();
	renderer.flush(FR_SCOPE);
	renderer.stencilWriteRef = ref;
}

Renderer::StencilWriteScope::~StencilWriteScope()
{
	Renderer& renderer = Renderer::inst();
	renderer.flush(FR_SCOPE);
	renderer.stencilWriteRef = -1;
}

Renderer::StencilTestScope::StencilTestScope(int ref)
{
	Renderer& renderer = Renderer::inst();
	renderer.flush(FR_SCOPE);
	renderer.stencilTestRef = ref;
}

Renderer::StencilTestScope::~StencilTestScope()
{
	Renderer& renderer = Renderer::inst();
	renderer.flush(FR_SCOPE);
	renderer.stencilTestRef = -1;
}

Renderer::DiscardTransparentScope::DiscardTransparentScope()
{
	Renderer& renderer = Renderer::inst();
	renderer.flush(FR_SCOPE);
	renderer.discardTransparent = true;
}

Renderer::DiscardTransparentScope::~DiscardTransparentScope()
{
	Renderer& renderer = Renderer::inst();
	renderer.flush(FR_SCOPE);
	renderer.discardTransparent = false;
}

Renderer::ScissorScope::ScissorScope(const Vec2i& position, const Vec2i& size)
{
	Renderer& renderer = Renderer::inst();
	renderer.flush(FR_SCOPE);
	previousOn = renderer.scissorOn;
	previousPosition = renderer.scissorPosition;
	previousSize = renderer.scissorSize;

	Vec2i lower = position, upper = position + size;
	if(previousOn)
	{
		lower.x = max(lower.x, previousPosition.x);
		lower.y = max(lower.y, previousPosition.y);
		upper.x = min(upper.x, previousPosition.x + previousSize.x);
		upper.y = min(upper.y, previousPosition.y + previousSize.y);
	}
	renderer.scissorOn = true;
	renderer.scissorPosition = lower;
	renderer.scissorSize = Vec2i(max(0, upper.x - lower.x), max(0, upper.y - lower.y));
}

Renderer::ScissorScope::~ScissorScope()
{
	Renderer& renderer = Renderer::inst();
	renderer.flush(FR_SCOPE);
	renderer.scissorOn = previousOn;
	renderer.scissorPosition = previousPosition;
	renderer.scissorSize = previousSize;
}

Renderer::DirectGL::DirectGL()
{
	Renderer::inst().flush(FR_DIRECT);
}

Renderer::DirectGL::~DirectGL()
{
	// What the raw code left in GL is its business; the next flush applies
	// everything again.
	Renderer::inst().invalidate();
}

// --- the read-back -----------------------------------------------------------

#if defined(BLOCKS5_TEST_HOOKS) && !defined(__EMSCRIPTEN__)
void Renderer::checkRecord()
{
	// After a draw: what GL is holding against what the renderer applied.
	// The first few disagreements are reported and the rest counted, or a
	// wrong record would flood the log at every flush.
	static int reported = 0;
	if(reported >= 20) return;

	const RenderState& s = streamState;
	std::string wrong;
	GLint value = 0;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &value);
	if(static_cast<uint>(value) != (s.texture.id ? s.texture.id : whiteTexture)) wrong += " texture";
	GLint blend[4];
	glGetIntegerv(GL_BLEND_SRC_RGB, &blend[0]);
	glGetIntegerv(GL_BLEND_DST_RGB, &blend[1]);
	glGetIntegerv(GL_BLEND_SRC_ALPHA, &blend[2]);
	glGetIntegerv(GL_BLEND_DST_ALPHA, &blend[3]);
	for(int i = 0; i < 4; i++)
		if(static_cast<GLenum>(blend[i]) != BLEND_FACTORS[s.blend][i]) { wrong += " blend"; break; }
	if(!glIsEnabled(GL_BLEND)) wrong += " blend-enable";
	glGetIntegerv(GL_CURRENT_PROGRAM, &value);
	if(static_cast<uint>(value) != program) wrong += " program";
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &value);
	if(static_cast<uint>(value) != vertexBuffer) wrong += " vertex-buffer";
	glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &value);
	if(static_cast<uint>(value) != indexBuffer) wrong += " index-buffer";
	GLboolean mask[4];
	glGetBooleanv(GL_COLOR_WRITEMASK, mask);
	for(int i = 0; i < 4; i++)
		if((mask[i] != 0) != colorMask[i]) { wrong += " color-mask"; break; }
	const bool stencilOn = glIsEnabled(GL_STENCIL_TEST) != 0;
	if(stencilOn != (stencilWriteRef >= 0 || stencilTestRef >= 0)) wrong += " stencil";
	const bool scissorIsOn = glIsEnabled(GL_SCISSOR_TEST) != 0;
	if(scissorIsOn != scissorOn) wrong += " scissor";
	else if(scissorOn)
	{
		GLint box[4];
		glGetIntegerv(GL_SCISSOR_BOX, box);
		if(box[0] != scissorPosition.x || box[1] != targetSize.y - scissorPosition.y - scissorSize.y ||
		   box[2] != scissorSize.x || box[3] != scissorSize.y) wrong += " scissor-box";
	}
	if(!wrong.empty())
	{
		reported++;
		printfLog("+ ERROR: the renderer's record disagrees with OpenGL after a draw:%s.\n", wrong.c_str());
	}
}
#else
void Renderer::checkRecord()
{
}
#endif
