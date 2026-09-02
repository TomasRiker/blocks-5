// gl_immediate.cpp - Gleichrichter fuer den Immediate-Modus im Web-Build.
//
// Emscriptens GL-Nachbildung baut je glBegin/glEnd-Block einen verschraenkten
// Vertexpuffer und rechnet daraus
//     numVertices = 4 * floatsWritten / bytesPerVertex,
// mit einer Zusicherung darauf, dass eine ganze Zahl herauskommt. Das gilt nur,
// wenn JEDER Vertex JEDES Attribut traegt. Das Spiel setzt aber, wie
// Fixed-Function-Code ueblicherweise, die Farbe einmal und gibt dann mehrere
// Vertices aus - 95 der 119 glBegin-Bloecke sehen so aus.
//
// Statt sie alle umzuschreiben, faengt diese Datei den Immediate-Modus ab, sammelt
// den Block und spielt ihn mit Farbe und Texturkoordinate an jedem Vertex wieder
// ab. Die eigentliche Arbeit macht weiterhin Emscripten, es bekommt nur einen
// gleichfoermigen Strom.
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
bool   blockHasTex = false;   // hat dieser Block ueberhaupt eine Texturkoordinate gesetzt?
GLenum blockMode        = GL_QUADS;
GLfloat currentR = 1.0f, currentG = 1.0f, currentB = 1.0f, currentA = 1.0f;
GLfloat currentS = 0.0f, currentT = 0.0f;
std::vector<Vertex> verts;

inline void addVertex(GLfloat x, GLfloat y, GLfloat z)
{
	if(!inBlock) return;          // ausserhalb eines Blocks bedeutet ein Vertex nichts
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
		// Texturkoordinaten nur ausgeben, wenn der Block sie auch benutzt hat -
		// sonst bekaemen untexturierte Primitive ein Attribut, das sie nie wollten.
		if(blockHasTex) emscripten_glTexCoord2f(i->s, i->t);
		emscripten_glVertex3f(i->x, i->y, i->z);
	}
	emscripten_glEnd();
	verts.clear();
}

// --- Farbe: wird immer mitgefuehrt und ausserhalb eines Blocks weitergereicht,
//     damit die "aktuelle Farbe" von GL auch fuer andere Geometrie stimmt.
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

// --- Texturkoordinaten
static inline void setTexCoord(GLfloat s, GLfloat t)
{
	currentS = s; currentT = t;
	if(inBlock) blockHasTex = true;
	// Ausserhalb eines Blocks absichtlich NICHT weitergereicht: Emscriptens
	// glTexCoord2i schreibt geradewegs in den Vertexpuffer und zaehlt
	// vertexCounter hoch, ohne zu pruefen, ob ein Block offen ist - das
	// verdirbt die Vertexzahl des naechsten Blocks.
}
void glTexCoord2f(GLfloat s, GLfloat t)     { setTexCoord(s, t); }
void glTexCoord2i(GLint s, GLint t)         { setTexCoord((GLfloat)s, (GLfloat)t); }
void glTexCoord2d(GLdouble s, GLdouble t)   { setTexCoord((GLfloat)s, (GLfloat)t); }
void glTexCoord2dv(const GLdouble* v)       { setTexCoord((GLfloat)v[0], (GLfloat)v[1]); }
void glTexCoord2iv(const GLint* v)          { setTexCoord((GLfloat)v[0], (GLfloat)v[1]); }

// --- Positionen
void glVertex2f(GLfloat x, GLfloat y)             { addVertex(x, y, 0.0f); }
void glVertex2i(GLint x, GLint y)                 { addVertex((GLfloat)x, (GLfloat)y, 0.0f); }
void glVertex2d(GLdouble x, GLdouble y)           { addVertex((GLfloat)x, (GLfloat)y, 0.0f); }
void glVertex2dv(const GLdouble* v)               { addVertex((GLfloat)v[0], (GLfloat)v[1], 0.0f); }
void glVertex3i(GLint x, GLint y, GLint z)        { addVertex((GLfloat)x, (GLfloat)y, (GLfloat)z); }

} // extern "C"
