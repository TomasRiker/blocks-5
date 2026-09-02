// gl_compat.cpp - Emscripten/WebGL compatibility shim for Blocks 5.
//
// Emscripten's -sLEGACY_GL_EMULATION covers most of the fixed-function pipeline this
// game relies on: glBegin/glEnd, the matrix stack, glColor4d, glVertex2i all work.
// Empirically 20 of the 80 GL entry points the game uses are declared in GL/gl.h but
// have no implementation, so linking fails on them. This file supplies them.
//
// Four kinds live here:
//   1. double/int variants  - forwarded to the float variants, which DO exist. Exact.
//   2. display lists        - STUBBED. Nothing that goes through a list will draw yet.
//   3. odds and ends        - stubbed or approximated; see the note on each.
//   4. corrections          - entry points Emscripten does implement, but wrongly.
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
    for (int i = 0; i < 16; ++i) params[i] = (GLdouble)tmp[i];
}

// --- 2. Display lists - STUB -------------------------------------------------
// WebGL has no display lists and Emscripten emulates none. Four sites use them:
//   level.cpp:1161   one list per tile layer (the whole tilemap)
//   font.cpp:21      one list per glyph
//   lightning.cpp:67 the thunderstorm bolt
//   cf_star.cpp:15   the star-wipe crossfade
// Until these are converted to draw directly, anything drawn through a list is
// invisible - expect a black or partial screen. Link-correct, render-incomplete.
GLAPI GLuint GLAPIENTRY glGenLists(GLsizei)          { return 1; }
GLAPI void   GLAPIENTRY glNewList(GLuint, GLenum)    {}
GLAPI void   GLAPIENTRY glEndList(void)              {}
GLAPI void   GLAPIENTRY glCallList(GLuint)           {}
GLAPI void   GLAPIENTRY glDeleteLists(GLuint, GLsizei) {}

// --- Unpack row length -------------------------------------------------------
// texture.cpp uploads a sub-rectangle of an SDL surface by setting
// GL_UNPACK_ROW_LENGTH to the surface pitch. WebGL 1.0 has no such parameter and
// raises INVALID_ENUM, then ignores it - so a surface whose pitch is wider than
// the texture would upload skewed. In practice SDL's 32-bit surfaces are always
// tightly packed, so this has not bitten, but relying on that silently is how
// you get a bug that only appears on someone else's machine. Track the value and
// repack the rows ourselves on the rare upload where it actually differs.
static GLint g_unpackRowLength = 0;

GLAPI void GLAPIENTRY glPixelStorei(GLenum pname, GLint param)
{
    if (pname == GL_UNPACK_ROW_LENGTH) { g_unpackRowLength = param; return; }
    emscripten_glPixelStorei(pname, param);
}

GLAPI void GLAPIENTRY glTexImage2D(GLenum target, GLint level, GLint internalFormat,
                                   GLsizei width, GLsizei height, GLint border,
                                   GLenum format, GLenum type, const GLvoid* p_pixels)
{
    const bool packed = (g_unpackRowLength == 0 || g_unpackRowLength == width);
    if (packed || !p_pixels || format != GL_RGBA || type != GL_UNSIGNED_BYTE)
    {
        emscripten_glTexImage2D(target, level, internalFormat, width, height, border, format, type, p_pixels);
        return;
    }

    // Rows are strided: copy them into a tight buffer first.
    const unsigned char* p_src = static_cast<const unsigned char*>(p_pixels);
    unsigned char* p_tight = new unsigned char[(size_t)width * height * 4];
    for (int y = 0; y < height; ++y)
        memcpy(p_tight + (size_t)y * width * 4,
               p_src + (size_t)y * g_unpackRowLength * 4,
               (size_t)width * 4);
    emscripten_glTexImage2D(target, level, internalFormat, width, height, border, format, type, p_tight);
    delete[] p_tight;
}

// --- 3. The attribute stack --------------------------------------------------
// Emscripten implements neither glPushAttrib nor glPopAttrib, and leaving them
// as no-ops is not survivable. Texture::bind() does:
//
//     glPushAttrib(GL_TRANSFORM_BIT);
//     glMatrixMode(GL_TEXTURE);
//     glLoadMatrixd(matrix);
//     glPopAttrib();              // this is what puts the mode back
//
// so with no-ops the matrix mode stays GL_TEXTURE after the first texture is
// bound, and every glPushMatrix/glTranslated in the game from then on
// transforms texture coordinates instead of geometry. Nothing errors; the
// screen just goes black.
//
// GL_ENABLE_BIT matters too. An earlier version of this file claimed it did
// not, on the grounds that Emscripten's glEnable returns early for
// GL_TEXTURE_2D. That reading was wrong: libglemu.js wraps glEnable TWICE.
// GLImmediate.setupHooks installs the outer wrapper, which calls
// TexEnvJIT.hook_enable(cap) and only then the inner GLEmulation one where the
// early return lives. The hook clears enabled_tex2D, and the generated
// fixed-function shader then drops its texture2D() call entirely - so
// glDisable(GL_TEXTURE_2D) IS real state and has to be restored. lava.cpp:150
// and teleporter.cpp:37 both depend on that.
//
// The game uses three masks: GL_TRANSFORM_BIT (6 sites, all bracketing a
// GL_TEXTURE matrix edit), GL_ENABLE_BIT (2 sites, both bracketing
// glDisable(GL_TEXTURE_2D)) and GL_ALL_ATTRIB_BITS (1 site, around
// Engine::presentFrame), so the mask is honoured rather than ignored.
static GLenum g_matrixMode = GL_MODELVIEW;

// The capabilities this game actually toggles, so a saved frame stays small.
static const GLenum kTrackedCaps[] = {
    GL_TEXTURE_2D, GL_BLEND, GL_ALPHA_TEST, GL_SCISSOR_TEST,
    GL_STENCIL_TEST, GL_CULL_FACE, GL_LINE_SMOOTH, GL_POINT_SMOOTH
};
static const int kNumTrackedCaps = (int)(sizeof(kTrackedCaps) / sizeof(kTrackedCaps[0]));

struct AttribFrame
{
    GLbitfield mask;
    GLenum     matrixMode;
    bool       enabled[8];
};
static AttribFrame g_attribStack[16];
static int  g_attribDepth = 0;
static bool g_capEnabled[8] = { false, false, false, false, false, false, false, false };

static int trackedCapIndex(GLenum cap)
{
    for (int i = 0; i < kNumTrackedCaps; ++i) if (kTrackedCaps[i] == cap) return i;
    return -1;
}

GLAPI void GLAPIENTRY glMatrixMode(GLenum mode)
{
    g_matrixMode = mode;
    emscripten_glMatrixMode(mode);
}

GLAPI void GLAPIENTRY glEnable(GLenum cap)
{
    const int i = trackedCapIndex(cap);
    if (i >= 0) g_capEnabled[i] = true;
    emscripten_glEnable(cap);
}

GLAPI void GLAPIENTRY glDisable(GLenum cap)
{
    const int i = trackedCapIndex(cap);
    if (i >= 0) g_capEnabled[i] = false;
    emscripten_glDisable(cap);
}

GLAPI void GLAPIENTRY glPushAttrib(GLbitfield mask)
{
    if (g_attribDepth < (int)(sizeof(g_attribStack) / sizeof(g_attribStack[0])))
    {
        AttribFrame& f = g_attribStack[g_attribDepth];
        f.mask = mask;
        f.matrixMode = g_matrixMode;
        for (int i = 0; i < kNumTrackedCaps; ++i) f.enabled[i] = g_capEnabled[i];
    }
    ++g_attribDepth;   // still counted when overflowing, so pops stay paired
}

GLAPI void GLAPIENTRY glPopAttrib(void)
{
    if (g_attribDepth <= 0) return;
    --g_attribDepth;
    if (g_attribDepth >= (int)(sizeof(g_attribStack) / sizeof(g_attribStack[0]))) return;

    const AttribFrame& f = g_attribStack[g_attribDepth];
    if (f.mask & (GL_TRANSFORM_BIT | GL_ALL_ATTRIB_BITS))
        glMatrixMode(f.matrixMode);
    if (f.mask & (GL_ENABLE_BIT | GL_ALL_ATTRIB_BITS))
        for (int i = 0; i < kNumTrackedCaps; ++i)
            if (f.enabled[i] != g_capEnabled[i])
                f.enabled[i] ? glEnable(kTrackedCaps[i]) : glDisable(kTrackedCaps[i]);
}

// Emscripten implements glReadBuffer and glDrawBuffer as abort(), so one call
// takes the whole runtime down. GL_BACK is the only buffer WebGL has, which
// makes a no-op exactly right. Engine::screenshot() reaches them today; the
// video recorder does too, once that feature comes back.
GLAPI void GLAPIENTRY glReadBuffer(GLenum) {}
GLAPI void GLAPIENTRY glDrawBuffer(GLenum) {}

// --- 4. gluLookAt ------------------------------------------------------------
// Emscripten's own gluLookAt does nothing at all. libglemu.js calls
//
//     mat4.lookAt(GLImmediate.matrix[cur], [ex,ey,ez], [cx,cy,cz], [ux,uy,uz])
//
// which is the gl-matrix 2.x convention of "destination first" - but the copy
// of gl-matrix bundled with Emscripten is 1.x, where the signature reads
// mat4.lookAt(eye, center, up, dest). So the current matrix goes in as the eye,
// every real argument shifts one place along, and the result is written into
// the three-element array that was meant to be the up vector and then dropped.
// The modelview matrix is never assigned: gluLookAt silently becomes a no-op
// and the camera stays wherever it was, which after the glLoadIdentity that
// precedes every call in this game means the origin.
//
// That single line is the whole of "the cube transition does nothing", "the
// level-end zoom neither rotates nor moves" (cf_cube.cpp, cf_zoom.cpp), and it
// takes cf_slices.cpp and the credits scene with it.
//
// glMultMatrixd is sound - mat4.multiply(current, m) post-multiplies, which is
// the GL order - so the textbook implementation on top of it is exact. This is
// Mesa's: forward and side normalised, up recomputed as side x forward so that
// a caller's up vector need not be perpendicular.
static void crossNorm(const double* p_a, const double* p_b, double* p_out, bool normalize)
{
    p_out[0] = p_a[1] * p_b[2] - p_a[2] * p_b[1];
    p_out[1] = p_a[2] * p_b[0] - p_a[0] * p_b[2];
    p_out[2] = p_a[0] * p_b[1] - p_a[1] * p_b[0];
    if (!normalize) return;
    const double len = sqrt(p_out[0] * p_out[0] + p_out[1] * p_out[1] + p_out[2] * p_out[2]);
    if (len > 0.0) { p_out[0] /= len; p_out[1] /= len; p_out[2] /= len; }
}

GLAPI void GLAPIENTRY gluLookAt(GLdouble eyeX, GLdouble eyeY, GLdouble eyeZ,
                                GLdouble centerX, GLdouble centerY, GLdouble centerZ,
                                GLdouble upX, GLdouble upY, GLdouble upZ)
{
    double f[3] = { centerX - eyeX, centerY - eyeY, centerZ - eyeZ };
    const double flen = sqrt(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
    if (flen > 0.0) { f[0] /= flen; f[1] /= flen; f[2] /= flen; }

    const double up[3] = { upX, upY, upZ };
    double s[3], u[3];
    crossNorm(f, up, s, true);
    crossNorm(s, f, u, false);

    // Column-major, the order glMultMatrixd reads: rows are side, up, -forward.
    const GLdouble m[16] = {
         s[0],  u[0], -f[0], 0.0,
         s[1],  u[1], -f[1], 0.0,
         s[2],  u[2], -f[2], 0.0,
          0.0,   0.0,   0.0, 1.0
    };
    glMultMatrixd(m);
    glTranslated(-eyeX, -eyeY, -eyeZ);
}

// gluPerspective is not broken, but it *replaces* the current matrix instead of
// multiplying into it. Every call site in this game does glLoadIdentity first,
// so the two agree today; going through glFrustum, which multiplies properly,
// means they still agree if one ever does not.
GLAPI void GLAPIENTRY gluPerspective(GLdouble fovy, GLdouble aspect,
                                     GLdouble zNear, GLdouble zFar)
{
    const GLdouble top = zNear * tan(fovy * 3.14159265358979323846 / 360.0);
    const GLdouble right = top * aspect;
    glFrustum(-right, right, -top, top, zNear, zFar);
}

// Dashed selection rectangles in the level editor: lines draw solid instead.
GLAPI void GLAPIENTRY glLineStipple(GLint, GLushort) {}
