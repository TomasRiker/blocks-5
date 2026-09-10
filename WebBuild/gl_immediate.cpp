// gl_immediate.cpp - rectifier for immediate mode in the web build.
//
// Emscripten's GL reimplementation builds one interleaved vertex buffer per
// glBegin/glEnd block and computes
//     numVertices = 4 * floatsWritten / bytesPerVertex,
// with an assertion that a whole number comes out. That holds only if EVERY
// vertex carries EVERY attribute. This game, as fixed-function code usually
// does, sets the colour once and then emits several vertices - 95 of the 119
// glBegin blocks look like that.
//
// Rather than rewrite them all, this file intercepts immediate mode, collects
// the block and replays it with a colour and a texture coordinate on every
// vertex. Emscripten still does the actual work; it just gets a uniform stream.
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

bool   inBlock     = false;
bool   blockHasTex = false;   // has this block set a texture coordinate at all?
GLenum blockMode        = GL_QUADS;
GLfloat currentR = 1.0f, currentG = 1.0f, currentB = 1.0f, currentA = 1.0f;
GLfloat currentS = 0.0f, currentT = 0.0f;
std::vector<Vertex> verts;

inline void addVertex(GLfloat x, GLfloat y, GLfloat z)
{
	if(!inBlock) return;          // outside a block a vertex means nothing
	const Vertex v = { x, y, z, currentR, currentG, currentB, currentA, currentS, currentT };
	verts.push_back(v);
}
} // namespace

extern "C" {

void glBegin(GLenum mode)
{
	blockMode = mode;
	inBlock = true;
	blockHasTex = false;
	verts.clear();
}

void glEnd(void)
{
	inBlock = false;
	if(verts.empty()) return;

	emscripten_glBegin(blockMode);
	for(std::vector<Vertex>::const_iterator i = verts.begin(); i != verts.end(); ++i)
	{
		emscripten_glColor4f(i->r, i->g, i->b, i->a);
		// Only emit texture coordinates if the block actually used them - or
		// untextured primitives would get an attribute they never wanted.
		if(blockHasTex) emscripten_glTexCoord2f(i->s, i->t);
		emscripten_glVertex3f(i->x, i->y, i->z);
	}
	emscripten_glEnd();
	verts.clear();
}

// --- Colour: always tracked, and passed on outside a block to keep GL's
//     "current colour" right for other geometry too.
static inline void setColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
	currentR = r; currentG = g; currentB = b; currentA = a;
	if(!inBlock) emscripten_glColor4f(r, g, b, a);
}
void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a)     { setColor(r, g, b, a); }
void glColor4d(GLdouble r, GLdouble g, GLdouble b, GLdouble a) { setColor((GLfloat)r, (GLfloat)g, (GLfloat)b, (GLfloat)a); }
void glColor3d(GLdouble r, GLdouble g, GLdouble b)             { setColor((GLfloat)r, (GLfloat)g, (GLfloat)b, 1.0f); }
void glColor4fv(const GLfloat* v)                              { setColor(v[0], v[1], v[2], v[3]); }
void glColor4dv(const GLdouble* v)                             { setColor((GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2], (GLfloat)v[3]); }
void glColor3dv(const GLdouble* v)                             { setColor((GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2], 1.0f); }

// --- Texture coordinates
static inline void setTexCoord(GLfloat s, GLfloat t)
{
	currentS = s; currentT = t;
	if(inBlock) blockHasTex = true;
	// Outside a block deliberately NOT passed on: Emscripten's glTexCoord2i
	// writes straight into the vertex buffer and counts vertexCounter up
	// without checking whether a block is open - which spoils the next
	// block's vertex count.
}
void glTexCoord2f(GLfloat s, GLfloat t)     { setTexCoord(s, t); }
void glTexCoord2i(GLint s, GLint t)         { setTexCoord((GLfloat)s, (GLfloat)t); }
void glTexCoord2d(GLdouble s, GLdouble t)   { setTexCoord((GLfloat)s, (GLfloat)t); }
void glTexCoord2dv(const GLdouble* v)       { setTexCoord((GLfloat)v[0], (GLfloat)v[1]); }
void glTexCoord2iv(const GLint* v)          { setTexCoord((GLfloat)v[0], (GLfloat)v[1]); }

// --- Positions
void glVertex2f(GLfloat x, GLfloat y)             { addVertex(x, y, 0.0f); }
void glVertex2i(GLint x, GLint y)                 { addVertex((GLfloat)x, (GLfloat)y, 0.0f); }
void glVertex2d(GLdouble x, GLdouble y)           { addVertex((GLfloat)x, (GLfloat)y, 0.0f); }
void glVertex2dv(const GLdouble* v)               { addVertex((GLfloat)v[0], (GLfloat)v[1], 0.0f); }
void glVertex3i(GLint x, GLint y, GLint z)        { addVertex((GLfloat)x, (GLfloat)y, (GLfloat)z); }

} // extern "C"
