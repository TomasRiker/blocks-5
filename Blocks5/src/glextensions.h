#ifndef GLEXTENSIONS_H
#define GLEXTENSIONS_H

// Die GL-Einstiegspunkte, die ueber GL 1.1 hinausgehen.
//
// Der Rest des Spiels benutzt reine Fixed-Function-Pipeline, die schon in GL 1.1
// steht und deshalb unter Windows direkt aus opengl32.dll kommt. Framebuffer
// Objects tun das nicht: unter Windows muessen sie ueber
// SDL_GL_GetProcAddress geholt werden, im Browser sind sie Kern von WebGL 1.
//
// Deswegen zwei Wege in einer Schnittstelle: unter Emscripten sind die Namen
// hier direkte Deklarationen der echten Funktionen, unter Windows
// Funktionszeiger, die init() fuellt.

namespace GLExtensions
{
	// Einmal aufrufen, nachdem der GL-Kontext steht. Liefert true, wenn
	// Framebuffer Objects benutzbar sind; sonst laeuft das Spiel wie frueher
	// direkt in den Backbuffer.
	bool init();

	bool haveFrameBufferObjects();

	// GL 2.0 / WebGL 1: alles, was die Praesentiershader brauchen.
	bool haveShaders();
}

// Die Konstanten sind in EXT_framebuffer_object und im GL-3.0-Kern identisch,
// nur anders benannt. SDL 1.2.15 liefert ein glext.h von 2011 mit; was darin
// fehlt, steht hier.
#ifndef GL_FRAMEBUFFER_EXT
#define GL_FRAMEBUFFER_EXT                0x8D40
#define GL_RENDERBUFFER_EXT               0x8D41
#define GL_COLOR_ATTACHMENT0_EXT          0x8CE0
#define GL_DEPTH_ATTACHMENT_EXT           0x8D00
#define GL_STENCIL_ATTACHMENT_EXT         0x8D20
#define GL_FRAMEBUFFER_COMPLETE_EXT       0x8CD5
#endif

// EXT_packed_depth_stencil ist juenger als das mitgelieferte glext.h.
#ifndef GL_DEPTH24_STENCIL8_EXT
#define GL_DEPTH24_STENCIL8_EXT           0x88F0
#endif
#ifndef GL_DEPTH_STENCIL_EXT
#define GL_DEPTH_STENCIL_EXT              0x84F9
#endif
#ifndef GL_DEPTH_STENCIL_ATTACHMENT_EXT
#define GL_DEPTH_STENCIL_ATTACHMENT_EXT   0x821A
#endif

#ifdef __EMSCRIPTEN__

// Kern von WebGL 1. GL/gl.h deklariert sie trotzdem nicht - das ist der
// GL-1.x-Header - und GLES2/gl2.h daneben zu legen wirft Typkonflikte. Also
// selbst deklarieren; gelinkt wird gegen Emscriptens GL-Bibliothek.
extern "C"
{
	void   glGenFramebuffers(GLsizei, GLuint*);
	void   glBindFramebuffer(GLenum, GLuint);
	void   glDeleteFramebuffers(GLsizei, const GLuint*);
	void   glFramebufferTexture2D(GLenum, GLenum, GLenum, GLuint, GLint);
	void   glGenRenderbuffers(GLsizei, GLuint*);
	void   glBindRenderbuffer(GLenum, GLuint);
	void   glDeleteRenderbuffers(GLsizei, const GLuint*);
	void   glRenderbufferStorage(GLenum, GLenum, GLsizei, GLsizei);
	void   glFramebufferRenderbuffer(GLenum, GLenum, GLenum, GLuint);
	GLenum glCheckFramebufferStatus(GLenum);
}

#define glExtGenFramebuffers         glGenFramebuffers
#define glExtBindFramebuffer         glBindFramebuffer
#define glExtDeleteFramebuffers      glDeleteFramebuffers
#define glExtFramebufferTexture2D    glFramebufferTexture2D
#define glExtGenRenderbuffers        glGenRenderbuffers
#define glExtBindRenderbuffer        glBindRenderbuffer
#define glExtDeleteRenderbuffers     glDeleteRenderbuffers
#define glExtRenderbufferStorage     glRenderbufferStorage
#define glExtFramebufferRenderbuffer glFramebufferRenderbuffer
#define glExtCheckFramebufferStatus  glCheckFramebufferStatus

extern "C"
{
	GLuint glCreateShader (GLenum);
	void glShaderSource (GLuint, GLsizei, const GLchar* const*, const GLint*);
	void glCompileShader (GLuint);
	void glGetShaderiv (GLuint, GLenum, GLint*);
	void glGetShaderInfoLog (GLuint, GLsizei, GLsizei*, GLchar*);
	void glDeleteShader (GLuint);
	GLuint glCreateProgram (void);
	void glAttachShader (GLuint, GLuint);
	void glBindAttribLocation (GLuint, GLuint, const GLchar*);
	void glLinkProgram (GLuint);
	void glGetProgramiv (GLuint, GLenum, GLint*);
	void glGetProgramInfoLog (GLuint, GLsizei, GLsizei*, GLchar*);
	void glUseProgram (GLuint);
	void glDeleteProgram (GLuint);
	GLint glGetUniformLocation (GLuint, const GLchar*);
	void glUniform1i (GLint, GLint);
	void glUniform1f (GLint, GLfloat);
	void glUniform2f (GLint, GLfloat, GLfloat);
	void glEnableVertexAttribArray (GLuint);
	void glDisableVertexAttribArray (GLuint);
	void glVertexAttribPointer (GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
	void glGenBuffers (GLsizei, GLuint*);
	void glBindBuffer (GLenum, GLuint);
	void glBufferData (GLenum, GLsizeiptr, const void*, GLenum);
	void glDeleteBuffers (GLsizei, const GLuint*);
}

#define glExtCreateShader             glCreateShader
#define glExtShaderSource             glShaderSource
#define glExtCompileShader            glCompileShader
#define glExtGetShaderiv              glGetShaderiv
#define glExtGetShaderInfoLog         glGetShaderInfoLog
#define glExtDeleteShader             glDeleteShader
#define glExtCreateProgram            glCreateProgram
#define glExtAttachShader             glAttachShader
#define glExtBindAttribLocation       glBindAttribLocation
#define glExtLinkProgram              glLinkProgram
#define glExtGetProgramiv             glGetProgramiv
#define glExtGetProgramInfoLog        glGetProgramInfoLog
#define glExtUseProgram               glUseProgram
#define glExtDeleteProgram            glDeleteProgram
#define glExtGetUniformLocation       glGetUniformLocation
#define glExtUniform1i                glUniform1i
#define glExtUniform1f                glUniform1f
#define glExtUniform2f                glUniform2f
#define glExtEnableVertexAttribArray  glEnableVertexAttribArray
#define glExtDisableVertexAttribArray glDisableVertexAttribArray
#define glExtVertexAttribPointer      glVertexAttribPointer
#define glExtGenBuffers               glGenBuffers
#define glExtBindBuffer               glBindBuffer
#define glExtBufferData               glBufferData
#define glExtDeleteBuffers            glDeleteBuffers

#else

extern PFNGLGENFRAMEBUFFERSEXTPROC         glExtGenFramebuffers;
extern PFNGLBINDFRAMEBUFFEREXTPROC         glExtBindFramebuffer;
extern PFNGLDELETEFRAMEBUFFERSEXTPROC      glExtDeleteFramebuffers;
extern PFNGLFRAMEBUFFERTEXTURE2DEXTPROC    glExtFramebufferTexture2D;
extern PFNGLGENRENDERBUFFERSEXTPROC        glExtGenRenderbuffers;
extern PFNGLBINDRENDERBUFFEREXTPROC        glExtBindRenderbuffer;
extern PFNGLDELETERENDERBUFFERSEXTPROC     glExtDeleteRenderbuffers;
extern PFNGLRENDERBUFFERSTORAGEEXTPROC     glExtRenderbufferStorage;
extern PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC glExtFramebufferRenderbuffer;
extern PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC  glExtCheckFramebufferStatus;

extern PFNGLCREATESHADERPROC                glExtCreateShader;
extern PFNGLSHADERSOURCEPROC                glExtShaderSource;
extern PFNGLCOMPILESHADERPROC               glExtCompileShader;
extern PFNGLGETSHADERIVPROC                 glExtGetShaderiv;
extern PFNGLGETSHADERINFOLOGPROC            glExtGetShaderInfoLog;
extern PFNGLDELETESHADERPROC                glExtDeleteShader;
extern PFNGLCREATEPROGRAMPROC               glExtCreateProgram;
extern PFNGLATTACHSHADERPROC                glExtAttachShader;
extern PFNGLBINDATTRIBLOCATIONPROC          glExtBindAttribLocation;
extern PFNGLLINKPROGRAMPROC                 glExtLinkProgram;
extern PFNGLGETPROGRAMIVPROC                glExtGetProgramiv;
extern PFNGLGETPROGRAMINFOLOGPROC           glExtGetProgramInfoLog;
extern PFNGLUSEPROGRAMPROC                  glExtUseProgram;
extern PFNGLDELETEPROGRAMPROC               glExtDeleteProgram;
extern PFNGLGETUNIFORMLOCATIONPROC          glExtGetUniformLocation;
extern PFNGLUNIFORM1IPROC                   glExtUniform1i;
extern PFNGLUNIFORM1FPROC                   glExtUniform1f;
extern PFNGLUNIFORM2FPROC                   glExtUniform2f;
extern PFNGLENABLEVERTEXATTRIBARRAYPROC     glExtEnableVertexAttribArray;
extern PFNGLDISABLEVERTEXATTRIBARRAYPROC    glExtDisableVertexAttribArray;
extern PFNGLVERTEXATTRIBPOINTERPROC         glExtVertexAttribPointer;
extern PFNGLGENBUFFERSPROC                  glExtGenBuffers;
extern PFNGLBINDBUFFERPROC                  glExtBindBuffer;
extern PFNGLBUFFERDATAPROC                  glExtBufferData;
extern PFNGLDELETEBUFFERSPROC               glExtDeleteBuffers;

#endif

#endif
