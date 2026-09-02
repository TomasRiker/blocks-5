// gl_compat.cpp - Ausgleichsschicht fuer Emscripten/WebGL.
//
// Emscriptens -sLEGACY_GL_EMULATION deckt den groessten Teil der
// Fixed-Function-Pipeline ab, die dieses Spiel benutzt: glBegin/glEnd, der
// Matrixstapel, glColor4d, glVertex2i. Zwanzig der achtzig benutzten
// GL-Einsprungpunkte sind aber nur deklariert und nirgends definiert, so dass
// das Linken an ihnen scheitert - die stehen hier.
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

// --- 2. Displaylisten ----------------------------------------------------------
// WebGL kennt keine, und Emscripten bildet keine nach. Die drei Stellen, die sie
// benutzen - font.cpp, level.cpp, lightning.cpp -, zeichnen im Browser direkt;
// diese Rumpffunktionen gibt es nur, damit gelinkt werden kann.
GLAPI GLuint GLAPIENTRY glGenLists(GLsizei)          { return 1; }
GLAPI void   GLAPIENTRY glNewList(GLuint, GLenum)    {}
GLAPI void   GLAPIENTRY glEndList(void)              {}
GLAPI void   GLAPIENTRY glCallList(GLuint)           {}
GLAPI void   GLAPIENTRY glDeleteLists(GLuint, GLsizei) {}

// --- Zeilenlaenge beim Hochladen -----------------------------------------------
// texture.cpp laedt einen Ausschnitt einer SDL-Flaeche hoch und setzt dafuer
// GL_UNPACK_ROW_LENGTH auf deren Schrittweite. WebGL 1.0 kennt den Parameter
// nicht und meldet INVALID_ENUM - eine Flaeche mit groesserer Schrittweite kaeme
// also schief an. Deshalb wird der Wert gemerkt und im Zweifel selbst umgepackt.
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

// --- 3. Der Attributstapel -----------------------------------------------------
// Emscripten kennt weder glPushAttrib noch glPopAttrib, und sie leer zu lassen
// waere nicht zu ueberleben: Texture::bind() setzt GL_TEXTURE als Matrixmodus und
// stellt ihn ueber glPopAttrib(GL_TRANSFORM_BIT) wieder zurueck. Ohne das bliebe
// der Modus stehen, und jedes glPushMatrix/glTranslated verschoebe von da an
// Texturkoordinaten statt Geometrie - ohne Fehlermeldung, nur schwarz.
//
// GL_ENABLE_BIT zaehlt ebenso. libglemu.js umschliesst glEnable zweimal; der
// aeussere Wrapper ruft TexEnvJIT.hook_enable(cap), und der loescht enabled_tex2D,
// worauf der erzeugte Shader seinen texture2D()-Aufruf ganz weglaesst.
// glDisable(GL_TEXTURE_2D) ist also echter Zustand und muss wiederhergestellt
// werden; lava.cpp und teleporter.cpp haengen daran.
//
// Das Spiel benutzt drei Masken - GL_TRANSFORM_BIT, GL_ENABLE_BIT und
// GL_ALL_ATTRIB_BITS -, deshalb wird die Maske beachtet und nicht ignoriert.
static GLenum g_matrixMode = GL_MODELVIEW;

// Nur die Faehigkeiten, die dieses Spiel wirklich umschaltet - so bleibt ein
// gesicherter Satz klein.
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
    ++g_attribDepth;   // beim Ueberlaufen mitgezaehlt, damit die Pops paarig bleiben
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

// Emscripten setzt glReadBuffer und glDrawBuffer als abort() um, ein einziger
// Aufruf risse also das Laufzeitsystem mit. GL_BACK ist der einzige Puffer, den
// WebGL hat - eine leere Funktion ist damit genau richtig.
GLAPI void GLAPIENTRY glReadBuffer(GLenum) {}
GLAPI void GLAPIENTRY glDrawBuffer(GLenum) {}

// --- 4. gluLookAt --------------------------------------------------------------
// Emscriptens eigenes gluLookAt tut gar nichts. libglemu.js ruft
//
//     mat4.lookAt(GLImmediate.matrix[cur], eye, center, up)
//
// - die Aufrufform von gl-matrix 2.x mit dem Ziel voran -, mitgeliefert ist aber
// gl-matrix 1.x mit mat4.lookAt(eye, center, up, dest). Jedes Argument rutscht
// eine Stelle weiter, das Ergebnis landet im dreielementigen up-Vektor und wird
// verworfen; die Modelview-Matrix wird nie zugewiesen. Das allein ist "der
// Wuerfeluebergang tut nichts" und "der Zoom am Levelende dreht sich nicht"
// (cf_cube.cpp, cf_zoom.cpp), und cf_slices.cpp und der Abspann haengen mit drin.
//
// glMultMatrixd stimmt - mat4.multiply(current, m) multipliziert von rechts, also
// in GL-Reihenfolge -, deshalb ist die Umsetzung darauf exakt. Es ist die von
// Mesa: forward und side normiert, up als side x forward neu berechnet, damit der
// up-Vektor des Aufrufers nicht senkrecht stehen muss.
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

    // Spaltenweise, so wie glMultMatrixd liest: die Zeilen sind side, up, -forward.
    const GLdouble m[16] = {
         s[0],  u[0], -f[0], 0.0,
         s[1],  u[1], -f[1], 0.0,
         s[2],  u[2], -f[2], 0.0,
          0.0,   0.0,   0.0, 1.0
    };
    glMultMatrixd(m);
    glTranslated(-eyeX, -eyeY, -eyeZ);
}

// gluPerspective ist nicht kaputt, ersetzt die aktuelle Matrix aber, statt in sie
// hineinzumultiplizieren. Ueber glFrustum, das richtig multipliziert, stimmt es
// auch dann noch, wenn ein Aufrufer einmal kein glLoadIdentity davorsetzt.
GLAPI void GLAPIENTRY gluPerspective(GLdouble fovy, GLdouble aspect,
                                     GLdouble zNear, GLdouble zFar)
{
    const GLdouble top = zNear * tan(fovy * 3.14159265358979323846 / 360.0);
    const GLdouble right = top * aspect;
    glFrustum(-right, right, -top, top, zNear, zFar);
}

// Dashed selection rectangles in the level editor: lines draw solid instead.
GLAPI void GLAPIENTRY glLineStipple(GLint, GLushort) {}
