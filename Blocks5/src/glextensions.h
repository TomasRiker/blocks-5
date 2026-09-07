#ifndef GLEXTENSIONS_H
#define GLEXTENSIONS_H

// The GL entry points that go beyond GL 1.1.
//
// The rest of the game uses the plain fixed-function pipeline, which is already
// in GL 1.1 and therefore comes straight out of opengl32.dll under Windows.
// Framebuffer objects do not: under Windows they have to be fetched through
// SDL_GL_GetProcAddress, in the browser they are core in WebGL 1.
//
// Hence two paths behind one interface: under Emscripten the names here are
// direct declarations of the real functions, under Windows function pointers
// that init() fills in.

namespace GLExtensions
{
	// Call once the GL context is up. Returns true if framebuffer objects are
	// usable; otherwise the game renders straight into the back buffer, as
	// before.
	bool init();

	bool haveFrameBufferObjects();

	// GL 2.0 / WebGL 1: everything the present shaders need.
	bool haveShaders();
}

// The constants are identical in EXT_framebuffer_object and in the GL 3.0 core,
// only named differently. SDL 1.2.15 ships a glext.h from 2011; what is missing
// from it is here.
#ifndef GL_FRAMEBUFFER_EXT
#define GL_FRAMEBUFFER_EXT                0x8D40
#define GL_RENDERBUFFER_EXT               0x8D41
#define GL_COLOR_ATTACHMENT0_EXT          0x8CE0
#define GL_DEPTH_ATTACHMENT_EXT           0x8D00
#define GL_STENCIL_ATTACHMENT_EXT         0x8D20
#define GL_FRAMEBUFFER_COMPLETE_EXT       0x8CD5
#endif

// EXT_packed_depth_stencil is younger than the shipped glext.h.
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

// Core in WebGL 1. GL/gl.h still does not declare them - that is the GL 1.x
// header - and putting GLES2/gl2.h beside it throws type conflicts. Declare
// them here instead; the link goes against Emscripten's GL library.
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
