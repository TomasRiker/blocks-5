// gl_immediate.cpp - immediate-mode normaliser for the web build.
//
// Emscripten's GL emulation builds one interleaved vertex buffer per
// glBegin/glEnd block and then computes
//     numVertices = 4 * floatsWritten / bytesPerVertex
// asserting that the result is a whole number. That only holds if EVERY vertex
// carries EVERY attribute. The game, like most fixed-function code, sets a
// colour once and then emits several vertices:
//
//     glBegin(GL_QUADS);
//     glColor4d(1,1,1,1);          // once
//     glTexCoord2i(0,0); glVertex2i(-256,-256);   // ...four times
//     ...
//     glEnd();                     // -> 2.5 vertices -> assertion failure
//
// 95 of the 119 glBegin blocks in this codebase are shaped that way, so rather
// than rewrite them all, this file intercepts immediate mode, buffers the block,
// and replays it with the current colour and texcoord attached to every vertex.
// Emscripten still does the real work (including GL_QUADS -> triangles); it just
// receives a uniform stream. The public glXxx names resolve here, and we forward
// to Emscripten's own emscripten_glXxx entry points.
#include <GL/gl.h>
#include <vector>

extern "C" {
void emscripten_glBegin(GLenum mode);
void emscripten_glEnd(void);
void emscripten_glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
void emscripten_glTexCoord2f(GLfloat s, GLfloat t);
void emscripten_glVertex3f(GLfloat x, GLfloat y, GLfloat z);
}

namespace {
struct Vertex { GLfloat x, y, z, r, g, b, a, s, t; };

bool   g_inBlock     = false;
bool   g_blockHasTex = false;   // did this block set a texcoord at all?
GLenum g_mode        = GL_QUADS;
GLfloat g_r = 1.0f, g_g = 1.0f, g_b = 1.0f, g_a = 1.0f;
GLfloat g_s = 0.0f, g_t = 0.0f;
std::vector<Vertex> g_verts;

inline void addVertex(GLfloat x, GLfloat y, GLfloat z)
{
    if (!g_inBlock) return;          // outside a block a bare vertex means nothing
    const Vertex v = { x, y, z, g_r, g_g, g_b, g_a, g_s, g_t };
    g_verts.push_back(v);
}
} // namespace

extern "C" {

void glBegin(GLenum mode)
{
    g_mode = mode;
    g_inBlock = true;
    g_blockHasTex = false;
    g_verts.clear();
}

void glEnd(void)
{
    g_inBlock = false;
    if (g_verts.empty()) return;

    emscripten_glBegin(g_mode);
    for (std::vector<Vertex>::const_iterator i = g_verts.begin(); i != g_verts.end(); ++i)
    {
        emscripten_glColor4f(i->r, i->g, i->b, i->a);
        // Only emit texcoords when the block actually used them, so untextured
        // primitives do not gain a texture attribute they never asked for.
        if (g_blockHasTex) emscripten_glTexCoord2f(i->s, i->t);
        emscripten_glVertex3f(i->x, i->y, i->z);
    }
    emscripten_glEnd();
    g_verts.clear();
}

// --- colour: tracked always; forwarded when outside a block so that the GL
//     "current colour" is still set for array-drawn and other geometry.
static inline void setColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    g_r = r; g_g = g; g_b = b; g_a = a;
    if (!g_inBlock) emscripten_glColor4f(r, g, b, a);
}
void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a)     { setColor(r, g, b, a); }
void glColor4d(GLdouble r, GLdouble g, GLdouble b, GLdouble a) { setColor((GLfloat)r, (GLfloat)g, (GLfloat)b, (GLfloat)a); }
void glColor3d(GLdouble r, GLdouble g, GLdouble b)             { setColor((GLfloat)r, (GLfloat)g, (GLfloat)b, 1.0f); }
void glColor4fv(const GLfloat* v)                              { setColor(v[0], v[1], v[2], v[3]); }
void glColor4dv(const GLdouble* v)                             { setColor((GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2], (GLfloat)v[3]); }
void glColor3dv(const GLdouble* v)                             { setColor((GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2], 1.0f); }

// --- texture coordinates
static inline void setTexCoord(GLfloat s, GLfloat t)
{
    g_s = s; g_t = t;
    if (g_inBlock) g_blockHasTex = true;
    // Deliberately NOT forwarded when outside a block: Emscripten's
    // glTexCoord2i writes straight into the immediate-mode vertex buffer and
    // bumps vertexCounter without checking whether a block is open (unlike
    // glColor4f, which guards on GLImmediate.mode >= 0), so forwarding here
    // corrupts the next block's vertex count.
}
void glTexCoord2f(GLfloat s, GLfloat t)     { setTexCoord(s, t); }
void glTexCoord2i(GLint s, GLint t)         { setTexCoord((GLfloat)s, (GLfloat)t); }
void glTexCoord2d(GLdouble s, GLdouble t)   { setTexCoord((GLfloat)s, (GLfloat)t); }
void glTexCoord2dv(const GLdouble* v)       { setTexCoord((GLfloat)v[0], (GLfloat)v[1]); }
void glTexCoord2iv(const GLint* v)          { setTexCoord((GLfloat)v[0], (GLfloat)v[1]); }

// --- positions
void glVertex2f(GLfloat x, GLfloat y)             { addVertex(x, y, 0.0f); }
void glVertex2i(GLint x, GLint y)                 { addVertex((GLfloat)x, (GLfloat)y, 0.0f); }
void glVertex2d(GLdouble x, GLdouble y)           { addVertex((GLfloat)x, (GLfloat)y, 0.0f); }
void glVertex2dv(const GLdouble* v)               { addVertex((GLfloat)v[0], (GLfloat)v[1], 0.0f); }
void glVertex3i(GLint x, GLint y, GLint z)        { addVertex((GLfloat)x, (GLfloat)y, (GLfloat)z); }

} // extern "C"
