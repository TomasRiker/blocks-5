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

	// Mesa's arithmetic for glRotated about z, so that a corner baked here
	// lands where its matrix stack put it: the angle to float first, the
	// radians in double, the sine and cosine in float - which is why a right
	// angle has a cosine of -4.4e-8 and not 0.
	void rotationTerms(double degrees, float* p_sin, float* p_cos)
	{
		const float angle = static_cast<float>(degrees);
		const double radians = angle * 3.14159265358979323846 / 180.0;
		*p_sin = sinf(static_cast<float>(radians));
		*p_cos = cosf(static_cast<float>(radians));
	}
}

Renderer::Renderer()
{
	texturingOn = false;
	directMatrixKnown = false;
	for(int i = 0; i < 16; i++) directMatrix[i] = (i % 5 == 0) ? 1.0f : 0.0f;
	colorMask[0] = colorMask[1] = colorMask[2] = colorMask[3] = true;
	stencilWriteRef = stencilTestRef = -1;
	discardTransparent = false;
	scopeDepth = 0;
	directDepth = 0;
	program = vertexBuffer = indexBuffer = whiteTexture = 0;
	uniformProjection = uniformTexture = uniformDiscard = -1;
	glKnown = false;
	glBinding = 0;
	glBindingKnown = false;
	glTexelScale = Vec2f(1.0f, 1.0f);
	glScaleKnown = false;
	glTexturing = -1;
	glBlend = BM_NORMAL;
	glBlendKnown = false;
	glWriteMask[0] = glWriteMask[1] = glWriteMask[2] = glWriteMask[3] = true;
	glStencilWriteRef = glStencilTestRef = -1;
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
		glBindTexture(GL_TEXTURE_2D, bound.id);
		glBinding = bound.id;
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

void Renderer::frameBegin()
{
	projection = Mat4::ortho(0.0f, 640.0f, 480.0f, 0.0f, -1.0f, 1.0f);
	projectionDirty = true;
	// Every frame starts on the identity: a push without its pop would
	// otherwise carry into the next frame, where nothing would explain it.
	transforms.resize(1);
	loadIdentity();
	glKnown = false;
}

void Renderer::frameEnd()
{
	flush(FR_FRAME);
}

void Renderer::setProjection(const Mat4& matrix)
{
	flush(FR_EXPLICIT);
	projection = matrix;
	projectionDirty = true;
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

void Renderer::applyTexelScale(const Vec2f& scale)
{
	// The fixed-function texture matrix, for raw code that writes texels:
	// a diagonal, and glScalef on the identity stores the two floats as
	// they are.
	if(glScaleKnown && glTexelScale == scale) return;
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glScalef(scale.x, scale.y, 1.0f);
	glMatrixMode(GL_MODELVIEW);
	glTexelScale = scale;
	glScaleKnown = true;
}

void Renderer::applyTexturing(bool on)
{
	const int want = on ? 1 : 0;
	if(glTexturing == want) return;
	if(on) glEnable(GL_TEXTURE_2D);
	else glDisable(GL_TEXTURE_2D);
	glTexturing = want;
}

void Renderer::applyBlendMode(BlendMode blend)
{
	if(glBlendKnown && glBlend == blend) return;
	applyBlend(blend);
	glBlend = blend;
	glBlendKnown = true;
}

// --- state -----------------------------------------------------------------

void Renderer::setTexture(const TextureRef& texture)
{
	bound = texture;
	if(texturingOn) current.texture = texture;
	bindReal(texture.id);
	if(directDepth) applyTexelScale(texture.texelScale);
}

void Renderer::setTexturing(bool on)
{
	texturingOn = on;
	current.texture = on ? bound : TextureRef();
	if(directDepth) applyTexturing(on);
}

void Renderer::setBlend(BlendMode blend)
{
	current.blend = blend;
	if(directDepth) applyBlendMode(blend);
}

void Renderer::deleteTexture(uint id)
{
	flush(FR_EXPLICIT);
	glDeleteTextures(1, &id);
	if(bound.id == id)
	{
		bound = TextureRef();
		if(texturingOn) current.texture = bound;
	}
	if(glBindingKnown && glBinding == id) glBinding = 0;
}

// --- the transform ---------------------------------------------------------

void Renderer::push()
{
	transforms.push_back(transforms.back());
	if(directDepth) { glPushMatrix(); directMatrixKnown = false; }
}

void Renderer::pop()
{
	if(transforms.size() > 1) transforms.pop_back();
	if(directDepth) { glPopMatrix(); directMatrixKnown = false; }
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
	if(directDepth) { glTranslated(x, y, 0.0); directMatrixKnown = false; }
}

void Renderer::scale(double x, double y)
{
	Transform& t = transforms.back();
	const float fx = static_cast<float>(x), fy = static_cast<float>(y);
	t.m00 *= fx; t.m10 *= fx;
	t.m01 *= fy; t.m11 *= fy;
	t.translationOnly = false;
	if(directDepth) { glScaled(x, y, 1.0); directMatrixKnown = false; }
}

void Renderer::rotate(double degrees)
{
	float s, c;
	rotationTerms(degrees, &s, &c);
	Transform& t = transforms.back();
	const float m00 = t.m00 * c + t.m01 * s;
	const float m01 = t.m00 * -s + t.m01 * c;
	const float m10 = t.m10 * c + t.m11 * s;
	const float m11 = t.m10 * -s + t.m11 * c;
	t.m00 = m00; t.m01 = m01; t.m10 = m10; t.m11 = m11;
	t.translationOnly = false;
	if(directDepth) { glRotated(degrees, 0.0, 0.0, 1.0); directMatrixKnown = false; }
}

void Renderer::loadIdentity()
{
	Transform& t = transforms.back();
	t.m00 = t.m11 = 1.0f;
	t.m01 = t.m10 = t.tx = t.ty = 0.0f;
	t.translationOnly = true;
	if(directDepth) { glLoadIdentity(); directMatrixKnown = false; }
}

void Renderer::bakePoint(double x, double y, float* p_outX, float* p_outY) const
{
	// The float matrix entries promote to double and the sum rounds once, to
	// float - the arithmetic Engine::queueSprite did with the matrix it read
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

void Renderer::submit(const RenderState& s, const double* p_x, const double* p_y,
					  const float* p_u, const float* p_v, const Vec4f* p_colors)
{
	requireState(s);

	// Inside a DirectGL the transform is GL's own modelview: the raw code
	// around this draw pushed and translated GL's stack, and the stack here
	// mirrors only what came through the renderer. Read once per call.
	if(directDepth && !directMatrixKnown)
	{
		glGetFloatv(GL_MODELVIEW_MATRIX, directMatrix);
		directMatrixKnown = true;
	}

	for(int i = 0; i < 4; i++)
	{
		Vertex vertex;
		if(directDepth)
		{
			const float* m = directMatrix;
			vertex.position = Vec2f(static_cast<float>(m[0] * p_x[i] + m[4] * p_y[i] + m[12]),
									static_cast<float>(m[1] * p_x[i] + m[5] * p_y[i] + m[13]));
		}
		else bakePoint(p_x[i], p_y[i], &vertex.position.x, &vertex.position.y);
		vertex.uv = Vec2f(p_u[i] * s.texture.texelScale.x, p_v[i] * s.texture.texelScale.y);
		vertex.color = p_colors[i];
		stream.push_back(vertex);
	}

	if(flushAll) flush(FR_EXPLICIT);
}

void Renderer::submitFlat(const double* p_x, const double* p_y, const Vec4f& color)
{
	const float u[4] = {BLOCK_U, BLOCK_U, BLOCK_U, BLOCK_U};
	const float v[4] = {BLOCK_V, BLOCK_V, BLOCK_V, BLOCK_V};
	const Vec4f colors[4] = {color, color, color, color};
	submit(flatState(), p_x, p_y, u, v, colors);
}

void Renderer::endCall()
{
	if(!directDepth) return;
	flush(FR_DIRECT);
	directMatrixKnown = false;
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
	endCall();
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
	endCall();
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
	endCall();
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
	endCall();
}

void Renderer::rect(const Vec2f& min, const Vec2f& max, const Vec4f& color)
{
	const double x[4] = {min.x, max.x, max.x, min.x};
	const double y[4] = {min.y, min.y, max.y, max.y};
	submitFlat(x, y, color);
	endCall();
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
	endCall();
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
	endCall();
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
	// The geometry LineDrawer built, so a beam looks as it did: one quad per
	// segment, half the width to either side, and at a turn of up to ninety
	// degrees a wedge on the outside of the corner - a quad with two corners
	// on the point, which is a triangle - between the two segments' ends.
	// A closed loop gets its last segment and no wedge at the seam.
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
	endCall();
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
	endCall();
}

// --- the flush ---------------------------------------------------------------

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

		if(!directDepth)
		{
			// Batched drawing owns the rare state: what the scopes say, and
			// nothing the raw code before it may have left on. Inside a
			// DirectGL the raw code's own enables apply to this draw as they
			// applied to its own - the scissor of a GUI window, say.
			glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
			applyStencil(stencilWriteRef, stencilTestRef);
			glDisable(GL_ALPHA_TEST);
			for(int i = 0; i < 4; i++) glWriteMask[i] = colorMask[i];
			glStencilWriteRef = stencilWriteRef;
			glStencilTestRef = stencilTestRef;
		}
		glKnown = true;
	}
	else if(!directDepth)
	{
		if(memcmp(glWriteMask, colorMask, sizeof(colorMask)))
		{
			glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
			for(int i = 0; i < 4; i++) glWriteMask[i] = colorMask[i];
		}
		if(glStencilWriteRef != stencilWriteRef || glStencilTestRef != stencilTestRef)
		{
			applyStencil(stencilWriteRef, stencilTestRef);
			glStencilWriteRef = stencilWriteRef;
			glStencilTestRef = stencilTestRef;
		}
	}

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

	// The binding is the bound texture again, which a raw upload or copy
	// after a bind relies on.
	bindReal(bound.id);

	if(directDepth)
	{
		// Back to the fixed function the raw code around this draw is using:
		// no program, no arrays, and the blend it set.
		glExtDisableVertexAttribArray(0);
		glExtDisableVertexAttribArray(1);
		glExtDisableVertexAttribArray(2);
		glExtBindBuffer(GL_ARRAY_BUFFER, 0);
		glExtBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		glExtUseProgram(0);
		applyBlendMode(current.blend);
		glKnown = false;
	}
}

void Renderer::clear(const Vec4f& color)
{
	flush(FR_EXPLICIT);
	glClearColor(color.r, color.g, color.b, color.a);
	glClear(GL_COLOR_BUFFER_BIT);
}

void Renderer::clearStencil()
{
	flush(FR_EXPLICIT);
	glClearStencil(0);
	glClear(GL_STENCIL_BUFFER_BIT);
}

void Renderer::invalidate()
{
	glKnown = false;
	glBindingKnown = false;
	glScaleKnown = false;
	glTexturing = -1;
	glBlendKnown = false;
	directMatrixKnown = false;
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
	renderer.scopeDepth++;
}

Renderer::ColorMaskScope::~ColorMaskScope()
{
	Renderer& renderer = Renderer::inst();
	renderer.flush(FR_SCOPE);
	memcpy(renderer.colorMask, previous, sizeof(previous));
	renderer.scopeDepth--;
}

Renderer::StencilWriteScope::StencilWriteScope(int ref)
{
	Renderer& renderer = Renderer::inst();
	renderer.flush(FR_SCOPE);
	renderer.stencilWriteRef = ref;
	renderer.scopeDepth++;
}

Renderer::StencilWriteScope::~StencilWriteScope()
{
	Renderer& renderer = Renderer::inst();
	renderer.flush(FR_SCOPE);
	renderer.stencilWriteRef = -1;
	renderer.scopeDepth--;
}

Renderer::StencilTestScope::StencilTestScope(int ref)
{
	Renderer& renderer = Renderer::inst();
	renderer.flush(FR_SCOPE);
	renderer.stencilTestRef = ref;
	renderer.scopeDepth++;
}

Renderer::StencilTestScope::~StencilTestScope()
{
	Renderer& renderer = Renderer::inst();
	renderer.flush(FR_SCOPE);
	renderer.stencilTestRef = -1;
	renderer.scopeDepth--;
}

Renderer::DiscardTransparentScope::DiscardTransparentScope()
{
	Renderer& renderer = Renderer::inst();
	renderer.flush(FR_SCOPE);
	renderer.discardTransparent = true;
	renderer.scopeDepth++;
}

Renderer::DiscardTransparentScope::~DiscardTransparentScope()
{
	Renderer& renderer = Renderer::inst();
	renderer.flush(FR_SCOPE);
	renderer.discardTransparent = false;
	renderer.scopeDepth--;
}

void Renderer::enterDirect()
{
	// The fixed function, as raw code expects to find it: no program and no
	// arrays of ours, the texture bound, scaled and enabled as the record
	// says, the blend set, and the rare state at its defaults. The modelview
	// is the caller's business: DirectGL loads the stack into it, Batched
	// never touched it.
	if(scopeDepth) printfLog("+ ERROR: raw GL begins with %d renderer scope(s) still open.\n", scopeDepth);
	glExtUseProgram(0);
	glExtDisableVertexAttribArray(0);
	glExtDisableVertexAttribArray(1);
	glExtDisableVertexAttribArray(2);
	glExtBindBuffer(GL_ARRAY_BUFFER, 0);
	glExtBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	applyTexelScale(bound.texelScale);
	bindReal(bound.id);
	applyTexturing(texturingOn);
	applyBlendMode(current.blend);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_ALPHA_TEST);
	glKnown = false;
	directMatrixKnown = false;
}

Renderer::DirectGL::DirectGL()
{
	Renderer& renderer = Renderer::inst();
	if(renderer.directDepth)
	{
		renderer.directDepth++;
		return;
	}

	// Flushed before the depth moves, so that what was queued goes up as
	// batched drawing: under the scopes' record, with the stencil and the
	// mask applied for it. A flush in direct mode leaves the rare state to
	// the raw code, and here that would be whatever the last batched draw
	// set - the lava's stencil test, on the shadows of the objects after it.
	renderer.flush(FR_DIRECT);
	renderer.directDepth = 1;
	renderer.enterDirect();

	// The stack goes into GL's modelview on top of a push of its own, so
	// that what the raw code around had there comes back at the end: a
	// bracket inside the level starts from the level's shake and must not
	// leave it behind for the screen that drew the level.
	const Transform& t = renderer.transforms.back();
	const float m[16] = {t.m00, t.m10, 0.0f, 0.0f,
						 t.m01, t.m11, 0.0f, 0.0f,
						 0.0f, 0.0f, 1.0f, 0.0f,
						 t.tx, t.ty, 0.0f, 1.0f};
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadMatrixf(m);
}

Renderer::DirectGL::~DirectGL()
{
	Renderer& renderer = Renderer::inst();
	if(--renderer.directDepth) return;
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	// What the raw code left in GL is its business; the next flush applies
	// everything again.
	renderer.glKnown = false;
	renderer.directMatrixKnown = false;
}

Renderer::Batched::Batched()
{
	Renderer& renderer = Renderer::inst();
	suspended = renderer.directDepth;
	if(!suspended) return;
	renderer.flush(FR_DIRECT);
	renderer.directDepth = 0;

	// The raw code around has pushed and translated GL's own stack; what
	// it draws next, batched, is relative to that.
	float m[16];
	glGetFloatv(GL_MODELVIEW_MATRIX, m);
	Transform t;
	t.m00 = m[0]; t.m01 = m[4]; t.tx = m[12];
	t.m10 = m[1]; t.m11 = m[5]; t.ty = m[13];
	t.translationOnly = (m[0] == 1.0f && m[5] == 1.0f && m[4] == 0.0f && m[1] == 0.0f);
	renderer.transforms.push_back(t);
	renderer.glKnown = false;
}

Renderer::Batched::~Batched()
{
	if(!suspended) return;
	Renderer& renderer = Renderer::inst();
	renderer.flush(FR_DIRECT);
	if(renderer.transforms.size() > 1) renderer.transforms.pop_back();
	renderer.directDepth = suspended;
	renderer.enterDirect();
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
	glGetIntegerv(GL_CURRENT_PROGRAM, &value);
	if(static_cast<uint>(value) != program) wrong += " program";
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &value);
	if(static_cast<uint>(value) != vertexBuffer) wrong += " vertex-buffer";
	glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &value);
	if(static_cast<uint>(value) != indexBuffer) wrong += " index-buffer";
	if(!directDepth)
	{
		GLboolean mask[4];
		glGetBooleanv(GL_COLOR_WRITEMASK, mask);
		for(int i = 0; i < 4; i++)
			if((mask[i] != 0) != colorMask[i]) { wrong += " color-mask"; break; }
		const bool stencilOn = glIsEnabled(GL_STENCIL_TEST) != 0;
		if(stencilOn != (stencilWriteRef >= 0 || stencilTestRef >= 0)) wrong += " stencil";
		if(glIsEnabled(GL_ALPHA_TEST)) wrong += " alpha-test";
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
