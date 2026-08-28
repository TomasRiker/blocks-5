// gl_compat.cpp - Emscripten/WebGL compatibility shim for Blocks 5.
//
// Emscripten's -sLEGACY_GL_EMULATION covers most of the fixed-function pipeline this
// game relies on: glBegin/glEnd, the matrix stack, glColor4d, glVertex2i all work.
// Empirically 20 of the 80 GL entry points the game uses are declared in GL/gl.h but
// have no implementation, so linking fails on them. This file supplies them.
//
// Three kinds live here:
//   1. double/int variants  - forwarded to the float variants, which DO exist. Exact.
//   2. display lists        - STUBBED. Nothing that goes through a list will draw yet.
//   3. odds and ends        - stubbed or approximated; see the note on each.
#include <GL/gl.h>
#include <GL/glu.h>

extern "C" void emscripten_glMatrixMode(GLenum mode);

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
// The game uses three masks: GL_TRANSFORM_BIT (6 sites, all bracketing a
// GL_TEXTURE matrix edit), GL_ENABLE_BIT (2 sites, both bracketing
// glDisable(GL_TEXTURE_2D)) and GL_ALL_ATTRIB_BITS (1 site, in the disabled
// hq2x path). Only the matrix mode has to be restored: Emscripten's glEnable /
// glDisable return early for GL_TEXTURE_2D and for every cap outside WebGL's
// set (see the capability switch in libglemu.js), so the enable state those
// two sites touch is not real state to begin with.
static GLenum g_matrixMode = GL_MODELVIEW;
static GLenum g_attribStack[16];
static int    g_attribDepth = 0;

GLAPI void GLAPIENTRY glMatrixMode(GLenum mode)
{
    g_matrixMode = mode;
    emscripten_glMatrixMode(mode);
}

GLAPI void GLAPIENTRY glPushAttrib(GLbitfield)
{
    if (g_attribDepth < (int)(sizeof(g_attribStack) / sizeof(g_attribStack[0])))
        g_attribStack[g_attribDepth] = g_matrixMode;
    ++g_attribDepth;   // still counted when overflowing, so pops stay paired
}

GLAPI void GLAPIENTRY glPopAttrib(void)
{
    if (g_attribDepth <= 0) return;
    --g_attribDepth;
    if (g_attribDepth < (int)(sizeof(g_attribStack) / sizeof(g_attribStack[0])))
        glMatrixMode(g_attribStack[g_attribDepth]);
}
// Dashed selection rectangles in the level editor: lines draw solid instead.
GLAPI void GLAPIENTRY glLineStipple(GLint, GLushort) {}
// engine.cpp uses these for the hq2x upscale blit, which is disabled anyway.
GLAPI void GLAPIENTRY glRasterPos2i(GLint, GLint) {}
GLAPI void GLAPIENTRY glDrawPixels(GLsizei, GLsizei, GLenum, GLenum, const GLvoid*) {}

// --- GLU tessellator - STUB (cf_star.cpp: one crossfade effect) --------------
GLAPI GLUtesselator* GLAPIENTRY gluNewTess(void)                          { return (GLUtesselator*)0; }
GLAPI void GLAPIENTRY gluDeleteTess(GLUtesselator*)                       {}
GLAPI void GLAPIENTRY gluTessCallback(GLUtesselator*, GLenum, _GLUfuncptr) {}
GLAPI void GLAPIENTRY gluTessBeginPolygon(GLUtesselator*, GLvoid*)        {}
GLAPI void GLAPIENTRY gluTessBeginContour(GLUtesselator*)                 {}
GLAPI void GLAPIENTRY gluTessVertex(GLUtesselator*, GLdouble*, GLvoid*)   {}
GLAPI void GLAPIENTRY gluTessEndContour(GLUtesselator*)                   {}
GLAPI void GLAPIENTRY gluTessEndPolygon(GLUtesselator*)                   {}
