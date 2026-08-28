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

// --- 3. Odds and ends --------------------------------------------------------
// The game brackets state changes with push/pop in 6 files. No-op means state
// leaks across those brackets; harmless for a link probe, revisit for fidelity.
GLAPI void GLAPIENTRY glPushAttrib(GLbitfield) {}
GLAPI void GLAPIENTRY glPopAttrib(void)        {}
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
