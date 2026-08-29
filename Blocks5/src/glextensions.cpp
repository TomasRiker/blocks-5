#include "pch.h"
#include "glextensions.h"

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
	// Erst den EXT-Namen, dann den Kernnamen. Treiber mit GL 3.0 fuehren beide,
	// aeltere nur den mit Suffix, und die Semantik ist fuer alles, was hier
	// benutzt wird, dieselbe.
	void* getProc(const char* p_name, const char* p_fallback)
	{
		void* p_proc = SDL_GL_GetProcAddress(p_name);
		if(!p_proc) p_proc = SDL_GL_GetProcAddress(p_fallback);
		return p_proc;
	}
}

#endif

namespace
{
	bool haveFBO = false;
	bool haveGLSL = false;
}

bool GLExtensions::init()
{
	haveFBO = false;

#ifdef __EMSCRIPTEN__

	// Kern von WebGL 1, es gibt nichts zu laden und nichts zu pruefen.
	haveFBO = true;
	printfLog("  Framebuffer objects are core in WebGL.\n");

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

	haveFBO = glExtGenFramebuffers && glExtBindFramebuffer && glExtDeleteFramebuffers &&
	          glExtFramebufferTexture2D && glExtGenRenderbuffers && glExtBindRenderbuffer &&
	          glExtDeleteRenderbuffers && glExtRenderbufferStorage &&
	          glExtFramebufferRenderbuffer && glExtCheckFramebufferStatus;

	// Die Erweiterungsliste ist nur eine Auskunft; entscheidend ist, ob alle
	// zehn Zeiger da sind. Ein Treiber, der die Namen fuehrt sie aber nicht
	// annonciert, ist brauchbar - umgekehrt nicht.
	printfLog("  Framebuffer objects: %s (extension string: %s)\n",
			  haveFBO ? "available" : "NOT available",
			  advertised ? "yes" : "no");

#endif

	// GL 2.0 fuer den Shader-Filter. Fehlt es, bleiben Nearest und Bilinear
	// uebrig - die brauchen nur eine Texturfiltereinstellung.
#ifdef __EMSCRIPTEN__
	haveGLSL = true;
#else
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

	haveGLSL = glExtCreateShader &&
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
	           glExtVertexAttribPointer &&
	           glExtGenBuffers &&
	           glExtBindBuffer &&
	           glExtBufferData &&
	           glExtDeleteBuffers;

	printfLog("  Shaders (GL 2.0):    %s\n", haveGLSL ? "available" : "NOT available");
#endif

	return haveFBO;
}

bool GLExtensions::haveShaders()
{
	return haveGLSL;
}

bool GLExtensions::haveFrameBufferObjects()
{
	return haveFBO;
}
