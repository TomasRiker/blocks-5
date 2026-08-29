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

	return haveFBO;
}

bool GLExtensions::haveFrameBufferObjects()
{
	return haveFBO;
}
