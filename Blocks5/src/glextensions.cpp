#include "pch.h"
#include "glextensions.h"
#include "fatalerror.h"

#ifndef __EMSCRIPTEN__

PFNGLGENFRAMEBUFFERSEXTPROC         glExtGenFramebuffers         = 0;
PFNGLBINDFRAMEBUFFEREXTPROC         glExtBindFramebuffer         = 0;
PFNGLDELETEFRAMEBUFFERSEXTPROC      glExtDeleteFramebuffers      = 0;
PFNGLFRAMEBUFFERTEXTURE2DEXTPROC    glExtFramebufferTexture2D    = 0;
PFNGLGENRENDERBUFFERSEXTPROC        glExtGenRenderbuffers        = 0;
PFNGLBINDRENDERBUFFEREXTPROC        glExtBindRenderbuffer        = 0;
PFNGLDELETERENDERBUFFERSEXTPROC     glExtDeleteRenderbuffers     = 0;
PFNGLRENDERBUFFERSTORAGEEXTPROC     glExtRenderbufferStorage     = 0;
PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC glExtFramebufferRenderbuffer = 0;
PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC  glExtCheckFramebufferStatus  = 0;

PFNGLCREATESHADERPROC                glExtCreateShader = 0;
PFNGLSHADERSOURCEPROC                glExtShaderSource = 0;
PFNGLCOMPILESHADERPROC               glExtCompileShader = 0;
PFNGLGETSHADERIVPROC                 glExtGetShaderiv = 0;
PFNGLGETSHADERINFOLOGPROC            glExtGetShaderInfoLog = 0;
PFNGLDELETESHADERPROC                glExtDeleteShader = 0;
PFNGLCREATEPROGRAMPROC               glExtCreateProgram = 0;
PFNGLATTACHSHADERPROC                glExtAttachShader = 0;
PFNGLBINDATTRIBLOCATIONPROC          glExtBindAttribLocation = 0;
PFNGLLINKPROGRAMPROC                 glExtLinkProgram = 0;
PFNGLGETPROGRAMIVPROC                glExtGetProgramiv = 0;
PFNGLGETPROGRAMINFOLOGPROC           glExtGetProgramInfoLog = 0;
PFNGLUSEPROGRAMPROC                  glExtUseProgram = 0;
PFNGLDELETEPROGRAMPROC               glExtDeleteProgram = 0;
PFNGLGETUNIFORMLOCATIONPROC          glExtGetUniformLocation = 0;
PFNGLUNIFORM1IPROC                   glExtUniform1i = 0;
PFNGLUNIFORM1FPROC                   glExtUniform1f = 0;
PFNGLUNIFORM2FPROC                   glExtUniform2f = 0;
PFNGLENABLEVERTEXATTRIBARRAYPROC     glExtEnableVertexAttribArray = 0;
PFNGLDISABLEVERTEXATTRIBARRAYPROC    glExtDisableVertexAttribArray = 0;
PFNGLVERTEXATTRIBPOINTERPROC         glExtVertexAttribPointer = 0;
PFNGLGENBUFFERSPROC                  glExtGenBuffers = 0;
PFNGLBINDBUFFERPROC                  glExtBindBuffer = 0;
PFNGLBUFFERDATAPROC                  glExtBufferData = 0;
PFNGLDELETEBUFFERSPROC               glExtDeleteBuffers = 0;

namespace
{
	// The EXT name first, then the core name. Drivers with GL 3.0 carry both,
	// older ones only the suffixed one, and the semantics are the same for
	// everything used here.
	void* getProc(const char* p_name, const char* p_fallback)
	{
		void* p_proc = SDL_GL_GetProcAddress(p_name);
		if(!p_proc) p_proc = SDL_GL_GetProcAddress(p_fallback);
		return p_proc;
	}

	// The game cannot run without any of these, so a missing one is the end
	// rather than something to report. The message is the whole of what the
	// player can act on: the OpenGL version and the renderer name say which
	// machine this is, and "GDI Generic" there is Windows saying no graphics
	// driver is in play at all - a fresh installation, safe mode, or a remote
	// desktop session. It is deliberately English: init() runs before
	// main() loads languages.txt, so there is no string table yet.
	void require(bool present, const char* p_what)
	{
		if(present) return;

		const char* p_version  = reinterpret_cast<const char*>(glGetString(GL_VERSION));
		const char* p_renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
		const char* p_vendor   = reinterpret_cast<const char*>(glGetString(GL_VENDOR));

		std::string message("Blocks 5 needs a graphics driver with OpenGL 2.0.\n\n");
		message += "Missing:   ";
		message += p_what;
		message += "\n\nOpenGL:    ";
		message += p_version  ? p_version  : "(none)";
		message += "\nRenderer:  ";
		message += p_renderer ? p_renderer : "(none)";
		message += "\nVendor:    ";
		message += p_vendor   ? p_vendor   : "(none)";
		message += "\n\nThis usually means that no graphics driver is installed, or\n"
		           "that the game is running through a remote desktop session.\n"
		           "Installing the driver for your graphics card should fix it.";

		fatalError("Blocks 5 - graphics driver too old", message);
	}
}

#endif

void GLExtensions::init()
{
#ifdef __EMSCRIPTEN__

	// Framebuffer objects, shaders and vertex buffers are all core in WebGL 1,
	// so there is nothing to load here and nothing that can be missing.
	printfLog("  GL: framebuffer objects, shaders and buffers are core in WebGL.\n");

#else

	const char* p_extensions = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
	const bool advertised = p_extensions &&
	                        (strstr(p_extensions, "GL_EXT_framebuffer_object") ||
	                         strstr(p_extensions, "GL_ARB_framebuffer_object"));

	glExtGenFramebuffers         = reinterpret_cast<PFNGLGENFRAMEBUFFERSEXTPROC>        (getProc("glGenFramebuffersEXT",         "glGenFramebuffers"));
	glExtBindFramebuffer         = reinterpret_cast<PFNGLBINDFRAMEBUFFEREXTPROC>        (getProc("glBindFramebufferEXT",         "glBindFramebuffer"));
	glExtDeleteFramebuffers      = reinterpret_cast<PFNGLDELETEFRAMEBUFFERSEXTPROC>     (getProc("glDeleteFramebuffersEXT",      "glDeleteFramebuffers"));
	glExtFramebufferTexture2D    = reinterpret_cast<PFNGLFRAMEBUFFERTEXTURE2DEXTPROC>   (getProc("glFramebufferTexture2DEXT",    "glFramebufferTexture2D"));
	glExtGenRenderbuffers        = reinterpret_cast<PFNGLGENRENDERBUFFERSEXTPROC>       (getProc("glGenRenderbuffersEXT",        "glGenRenderbuffers"));
	glExtBindRenderbuffer        = reinterpret_cast<PFNGLBINDRENDERBUFFEREXTPROC>       (getProc("glBindRenderbufferEXT",        "glBindRenderbuffer"));
	glExtDeleteRenderbuffers     = reinterpret_cast<PFNGLDELETERENDERBUFFERSEXTPROC>    (getProc("glDeleteRenderbuffersEXT",     "glDeleteRenderbuffers"));
	glExtRenderbufferStorage     = reinterpret_cast<PFNGLRENDERBUFFERSTORAGEEXTPROC>    (getProc("glRenderbufferStorageEXT",     "glRenderbufferStorage"));
	glExtFramebufferRenderbuffer = reinterpret_cast<PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC>(getProc("glFramebufferRenderbufferEXT", "glFramebufferRenderbuffer"));
	glExtCheckFramebufferStatus  = reinterpret_cast<PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC> (getProc("glCheckFramebufferStatusEXT",  "glCheckFramebufferStatus"));

	// The extension list is only information; what decides is whether all ten
	// pointers are there. A driver that carries the names but does not
	// advertise them is usable - the other way round is not.
	printfLog("  Framebuffer objects: extension string says %s\n", advertised ? "yes" : "no");
	require(glExtGenFramebuffers && glExtBindFramebuffer && glExtDeleteFramebuffers &&
	        glExtFramebufferTexture2D && glExtGenRenderbuffers && glExtBindRenderbuffer &&
	        glExtDeleteRenderbuffers && glExtRenderbufferStorage &&
	        glExtFramebufferRenderbuffer && glExtCheckFramebufferStatus,
	        "framebuffer objects");

	// GL 2.0: the shaders the present filters are, and the vertex buffers they
	// draw out of.
	glExtCreateShader             = reinterpret_cast<PFNGLCREATESHADERPROC>(SDL_GL_GetProcAddress("glCreateShader"));
	glExtShaderSource             = reinterpret_cast<PFNGLSHADERSOURCEPROC>(SDL_GL_GetProcAddress("glShaderSource"));
	glExtCompileShader            = reinterpret_cast<PFNGLCOMPILESHADERPROC>(SDL_GL_GetProcAddress("glCompileShader"));
	glExtGetShaderiv              = reinterpret_cast<PFNGLGETSHADERIVPROC>(SDL_GL_GetProcAddress("glGetShaderiv"));
	glExtGetShaderInfoLog         = reinterpret_cast<PFNGLGETSHADERINFOLOGPROC>(SDL_GL_GetProcAddress("glGetShaderInfoLog"));
	glExtDeleteShader             = reinterpret_cast<PFNGLDELETESHADERPROC>(SDL_GL_GetProcAddress("glDeleteShader"));
	glExtCreateProgram            = reinterpret_cast<PFNGLCREATEPROGRAMPROC>(SDL_GL_GetProcAddress("glCreateProgram"));
	glExtAttachShader             = reinterpret_cast<PFNGLATTACHSHADERPROC>(SDL_GL_GetProcAddress("glAttachShader"));
	glExtBindAttribLocation       = reinterpret_cast<PFNGLBINDATTRIBLOCATIONPROC>(SDL_GL_GetProcAddress("glBindAttribLocation"));
	glExtLinkProgram              = reinterpret_cast<PFNGLLINKPROGRAMPROC>(SDL_GL_GetProcAddress("glLinkProgram"));
	glExtGetProgramiv             = reinterpret_cast<PFNGLGETPROGRAMIVPROC>(SDL_GL_GetProcAddress("glGetProgramiv"));
	glExtGetProgramInfoLog        = reinterpret_cast<PFNGLGETPROGRAMINFOLOGPROC>(SDL_GL_GetProcAddress("glGetProgramInfoLog"));
	glExtUseProgram               = reinterpret_cast<PFNGLUSEPROGRAMPROC>(SDL_GL_GetProcAddress("glUseProgram"));
	glExtDeleteProgram            = reinterpret_cast<PFNGLDELETEPROGRAMPROC>(SDL_GL_GetProcAddress("glDeleteProgram"));
	glExtGetUniformLocation       = reinterpret_cast<PFNGLGETUNIFORMLOCATIONPROC>(SDL_GL_GetProcAddress("glGetUniformLocation"));
	glExtUniform1i                = reinterpret_cast<PFNGLUNIFORM1IPROC>(SDL_GL_GetProcAddress("glUniform1i"));
	glExtUniform1f                = reinterpret_cast<PFNGLUNIFORM1FPROC>(SDL_GL_GetProcAddress("glUniform1f"));
	glExtUniform2f                = reinterpret_cast<PFNGLUNIFORM2FPROC>(SDL_GL_GetProcAddress("glUniform2f"));
	glExtEnableVertexAttribArray  = reinterpret_cast<PFNGLENABLEVERTEXATTRIBARRAYPROC>(SDL_GL_GetProcAddress("glEnableVertexAttribArray"));
	glExtDisableVertexAttribArray = reinterpret_cast<PFNGLDISABLEVERTEXATTRIBARRAYPROC>(SDL_GL_GetProcAddress("glDisableVertexAttribArray"));
	glExtVertexAttribPointer      = reinterpret_cast<PFNGLVERTEXATTRIBPOINTERPROC>(SDL_GL_GetProcAddress("glVertexAttribPointer"));
	glExtGenBuffers               = reinterpret_cast<PFNGLGENBUFFERSPROC>(SDL_GL_GetProcAddress("glGenBuffers"));
	glExtBindBuffer               = reinterpret_cast<PFNGLBINDBUFFERPROC>(SDL_GL_GetProcAddress("glBindBuffer"));
	glExtBufferData               = reinterpret_cast<PFNGLBUFFERDATAPROC>(SDL_GL_GetProcAddress("glBufferData"));
	glExtDeleteBuffers            = reinterpret_cast<PFNGLDELETEBUFFERSPROC>(SDL_GL_GetProcAddress("glDeleteBuffers"));

	require(glExtCreateShader &&
	        glExtShaderSource &&
	        glExtCompileShader &&
	        glExtGetShaderiv &&
	        glExtGetShaderInfoLog &&
	        glExtDeleteShader &&
	        glExtCreateProgram &&
	        glExtAttachShader &&
	        glExtBindAttribLocation &&
	        glExtLinkProgram &&
	        glExtGetProgramiv &&
	        glExtGetProgramInfoLog &&
	        glExtUseProgram &&
	        glExtDeleteProgram &&
	        glExtGetUniformLocation &&
	        glExtUniform1i &&
	        glExtUniform1f &&
	        glExtUniform2f &&
	        glExtEnableVertexAttribArray &&
	        glExtDisableVertexAttribArray &&
	        glExtVertexAttribPointer,
	        "shaders (OpenGL 2.0)");
	require(glExtGenBuffers && glExtBindBuffer && glExtBufferData && glExtDeleteBuffers,
	        "vertex buffer objects");
#endif
}
