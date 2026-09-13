// gl_compat.cpp - compatibility layer for Emscripten/WebGL.
//
// Emscripten's -sLEGACY_GL_EMULATION covers most of the fixed-function
// pipeline this game uses: glBegin/glEnd, the matrix stack, glColor4d,
// glVertex2i. Thirteen of the GL entry points the game calls are only declared
// and never defined, though, which makes the link fail on them - those are the
// thirteen here.
#include <GL/gl.h>
#include <GL/glu.h>
#include <cstring>
#include <cmath>

extern "C" void emscripten_glMatrixMode(GLenum mode);
extern "C" void emscripten_glEnable(GLenum cap);
extern "C" void emscripten_glDisable(GLenum cap);
extern "C" void emscripten_glPixelStorei(GLenum pname, GLint param);
extern "C" void emscripten_glTexImage2D(GLenum target, GLint level, GLint internalFormat,
										GLsizei width, GLsizei height, GLint border,
										GLenum format, GLenum type, const GLvoid* pixels);

// (immediate-mode variants moved to gl_immediate.cpp)

GLAPI void GLAPIENTRY glGetDoublev(GLenum pname, GLdouble* params) {
	GLfloat tmp[16] = {0};
	glGetFloatv(pname, tmp);
	for(int i = 0; i < 16; ++i) params[i] = (GLdouble)tmp[i];
}

// --- Row length on upload ------------------------------------------------------
// texture.cpp uploads a region of an SDL surface and sets GL_UNPACK_ROW_LENGTH
// to that surface's stride. WebGL 1.0 does not know the parameter and reports
// INVALID_ENUM, and a surface with a larger stride would then arrive skewed.
// The value is therefore remembered, and the rows repacked here when needed.
static GLint unpackRowLength = 0;

GLAPI void GLAPIENTRY glPixelStorei(GLenum pname, GLint param)
{
	if(pname == GL_UNPACK_ROW_LENGTH) { unpackRowLength = param; return; }
	emscripten_glPixelStorei(pname, param);
}

GLAPI void GLAPIENTRY glTexImage2D(GLenum target, GLint level, GLint internalFormat,
                                   GLsizei width, GLsizei height, GLint border,
                                   GLenum format, GLenum type, const GLvoid* p_pixels)
{
	const bool packed = (unpackRowLength == 0 || unpackRowLength == width);
	if(packed || !p_pixels || format != GL_RGBA || type != GL_UNSIGNED_BYTE)
	{
		emscripten_glTexImage2D(target, level, internalFormat, width, height, border, format, type, p_pixels);
		return;
	}

	// Rows are strided: copy them into a tight buffer first.
	const unsigned char* p_src = static_cast<const unsigned char*>(p_pixels);
	unsigned char* p_tight = new unsigned char[(size_t)width * height * 4];
	for(int y = 0; y < height; ++y)
		memcpy(p_tight + (size_t)y * width * 4,
		       p_src + (size_t)y * unpackRowLength * 4,
		       (size_t)width * 4);
	emscripten_glTexImage2D(target, level, internalFormat, width, height, border, format, type, p_tight);
	delete[] p_tight;
}

// --- The attribute stack ----------------------------------------------------
// Emscripten has neither glPushAttrib nor glPopAttrib, and leaving them empty
// would not be survivable: Texture::bind() sets GL_TEXTURE as the matrix mode
// and restores it through glPopAttrib(GL_TRANSFORM_BIT). Without that the mode
// would stay, and from then on every glPushMatrix/glTranslated would move
// texture coordinates instead of geometry - with no error, just black.
//
// GL_ENABLE_BIT counts just as much. libglemu.js wraps glEnable twice; the
// outer wrapper calls TexEnvJIT.hook_enable(cap), which clears enabled_tex2D,
// whereupon the generated shader leaves its texture2D() call out entirely.
// glDisable(GL_TEXTURE_2D) is therefore real state and has to be restored;
// lava.cpp and teleporter.cpp depend on it.
//
// The game uses three masks - GL_TRANSFORM_BIT, GL_ENABLE_BIT and
// GL_ALL_ATTRIB_BITS - which is why the mask is honoured, not ignored. Test
// each bit on its own and never against GL_ALL_ATTRIB_BITS: that one is
// 0x000FFFFF and contains both of the others, so "mask & (GL_ENABLE_BIT |
// GL_ALL_ATTRIB_BITS)" is "mask != 0" and every push would restore
// everything. Desktop GL restores exactly what it was asked to, and a
// browser that restores more is a divergence nothing on this machine can
// see.
static GLenum currentMatrixMode = GL_MODELVIEW;

// Only the capabilities this game actually toggles - that keeps each saved
// set small.
static const GLenum trackedCaps[] = {
	GL_TEXTURE_2D, GL_BLEND, GL_ALPHA_TEST, GL_SCISSOR_TEST,
	GL_STENCIL_TEST, GL_CULL_FACE, GL_LINE_SMOOTH, GL_POINT_SMOOTH
};
static const int numTrackedCaps = (int)(sizeof(trackedCaps) / sizeof(trackedCaps[0]));

struct AttribFrame
{
	GLbitfield mask;
	GLenum     matrixMode;
	bool       enabled[8];
};
static AttribFrame attribStack[16];
static int  attribDepth = 0;
static bool capEnabled[8] = { false, false, false, false, false, false, false, false };

static int trackedCapIndex(GLenum cap)
{
	for(int i = 0; i < numTrackedCaps; ++i) if(trackedCaps[i] == cap) return i;
	return -1;
}

GLAPI void GLAPIENTRY glMatrixMode(GLenum mode)
{
	currentMatrixMode = mode;
	emscripten_glMatrixMode(mode);
}

GLAPI void GLAPIENTRY glEnable(GLenum cap)
{
	const int i = trackedCapIndex(cap);
	if(i >= 0) capEnabled[i] = true;
	emscripten_glEnable(cap);
}

GLAPI void GLAPIENTRY glDisable(GLenum cap)
{
	const int i = trackedCapIndex(cap);
	if(i >= 0) capEnabled[i] = false;
	emscripten_glDisable(cap);
}

GLAPI void GLAPIENTRY glPushAttrib(GLbitfield mask)
{
	if(attribDepth < (int)(sizeof(attribStack) / sizeof(attribStack[0])))
	{
		AttribFrame& f = attribStack[attribDepth];
		f.mask = mask;
		f.matrixMode = currentMatrixMode;
		for(int i = 0; i < numTrackedCaps; ++i) f.enabled[i] = capEnabled[i];
	}
	++attribDepth;   // counted on overflow too, so the pops stay paired
}

GLAPI void GLAPIENTRY glPopAttrib(void)
{
	if(attribDepth <= 0) return;
	--attribDepth;
	if(attribDepth >= (int)(sizeof(attribStack) / sizeof(attribStack[0]))) return;

	const AttribFrame& f = attribStack[attribDepth];
	if(f.mask & GL_TRANSFORM_BIT)
		glMatrixMode(f.matrixMode);
	if(f.mask & GL_ENABLE_BIT)
		for(int i = 0; i < numTrackedCaps; ++i)
			if(f.enabled[i] != capEnabled[i])
				f.enabled[i] ? glEnable(trackedCaps[i]) : glDisable(trackedCaps[i]);
}

// Emscripten implements glReadBuffer and glDrawBuffer as abort(), and a single
// call would take the runtime down with it. GL_BACK is the only buffer WebGL
// has, which makes an empty function exactly right.
GLAPI void GLAPIENTRY glReadBuffer(GLenum) {}
GLAPI void GLAPIENTRY glDrawBuffer(GLenum) {}

// --- gluLookAt --------------------------------------------------------------
// Emscripten's own gluLookAt does nothing at all. libglemu.js calls
//
//     mat4.lookAt(GLImmediate.matrix[cur], eye, center, up)
//
// - the gl-matrix 2.x call shape, destination first - but what ships is
// gl-matrix 1.x with mat4.lookAt(eye, center, up, dest). Every argument slides
// one place along, the result lands in the three-element up vector and is
// discarded; the modelview matrix is never assigned. That alone is "the cube
// transition does nothing" and "the zoom at the end of a level does not turn"
// (cf_cube.cpp, cf_zoom.cpp), with cf_slices.cpp and the credits caught in it.
//
// glMultMatrixd is correct - mat4.multiply(current, m) multiplies from the
// right, i.e. in GL order - which makes the implementation built on it exact.
// It is Mesa's: forward and side normalized, up recomputed as side x forward,
// and the caller's up vector therefore does not have to be perpendicular.
static void crossNorm(const double* p_a, const double* p_b, double* p_out, bool normalize)
{
	p_out[0] = p_a[1] * p_b[2] - p_a[2] * p_b[1];
	p_out[1] = p_a[2] * p_b[0] - p_a[0] * p_b[2];
	p_out[2] = p_a[0] * p_b[1] - p_a[1] * p_b[0];
	if(!normalize) return;
	const double len = sqrt(p_out[0] * p_out[0] + p_out[1] * p_out[1] + p_out[2] * p_out[2]);
	if(len > 0.0) { p_out[0] /= len; p_out[1] /= len; p_out[2] /= len; }
}

GLAPI void GLAPIENTRY gluLookAt(GLdouble eyeX, GLdouble eyeY, GLdouble eyeZ,
								GLdouble centerX, GLdouble centerY, GLdouble centerZ,
								GLdouble upX, GLdouble upY, GLdouble upZ)
{
	double f[3] = { centerX - eyeX, centerY - eyeY, centerZ - eyeZ };
	const double flen = sqrt(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
	if(flen > 0.0) { f[0] /= flen; f[1] /= flen; f[2] /= flen; }

	const double up[3] = { upX, upY, upZ };
	double s[3], u[3];
	crossNorm(f, up, s, true);
	crossNorm(s, f, u, false);

	// Column by column, as glMultMatrixd reads: the rows are side, up, -forward.
	const GLdouble m[16] = {
	     s[0],  u[0], -f[0], 0.0,
	     s[1],  u[1], -f[1], 0.0,
	     s[2],  u[2], -f[2], 0.0,
	      0.0,   0.0,   0.0, 1.0
	};
	glMultMatrixd(m);
	glTranslated(-eyeX, -eyeY, -eyeZ);
}

// gluPerspective is not broken, but it replaces the current matrix instead of
// multiplying into it. Going through glFrustum, which multiplies correctly,
// keeps it right even where a caller does not put a glLoadIdentity in front.
GLAPI void GLAPIENTRY gluPerspective(GLdouble fovy, GLdouble aspect,
                                     GLdouble zNear, GLdouble zFar)
{
	const GLdouble top = zNear * tan(fovy * 3.14159265358979323846 / 360.0);
	const GLdouble right = top * aspect;
	glFrustum(-right, right, -top, top, zNear, zFar);
}

// Dashed selection rectangles in the level editor: lines draw solid instead.
GLAPI void GLAPIENTRY glLineStipple(GLint, GLushort) {}
