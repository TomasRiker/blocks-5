// linux_window.cpp - the fullscreen switch under X11.
//
// Its own translation unit, because <X11/Xlib.h> comes in here: it makes Font,
// Window, Screen and Cursor type names of its own, and engine.cpp uses the
// game's classes of the same name right after the include.
#include <SDL.h>
#include <SDL_syswm.h>
#include <cstring>
#ifdef SDL_VIDEO_DRIVER_X11
// XSizeHints and XSetWMNormalHints are not in the Xlib.h that SDL_syswm.h
// brings along, but one header further on.
#include <X11/Xutil.h>
#endif
#include "linux_window.h"

namespace LinuxWindow
{

bool setFullScreen(bool wantFullScreen)
{
#ifdef SDL_VIDEO_DRIVER_X11
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if(!SDL_GetWMInfo(&info) || info.subsystem != SDL_SYSWM_X11 || !info.info.x11.display) return false;

	Display* p_display = info.info.x11.display;

	// SDL draws from a thread of its own; every Xlib call from outside belongs
	// between these two.
	if(info.info.x11.lock_func) info.info.x11.lock_func();

	// A program does not put its own window into fullscreen under X11 - it
	// tells the window manager that it wants one, and the window manager
	// decides on size and position. The way to do that is this message to the
	// root window, as described in the EWMH; every window manager of the last
	// twenty years understands it. XMoveResizeWindow instead would go past the
	// window manager and behave differently under every single one.
	XEvent event;
	memset(&event, 0, sizeof(event));
	event.type                 = ClientMessage;
	event.xclient.window       = info.info.x11.window;
	event.xclient.message_type = XInternAtom(p_display, "_NET_WM_STATE", False);
	event.xclient.format       = 32;
	event.xclient.data.l[0]    = wantFullScreen ? 1 : 0;   // _NET_WM_STATE_ADD / _REMOVE
	event.xclient.data.l[1]    = XInternAtom(p_display, "_NET_WM_STATE_FULLSCREEN", False);
	event.xclient.data.l[2]    = 0;
	event.xclient.data.l[3]    = 1;                        // from the application, not from a pager
	XSendEvent(p_display, DefaultRootWindow(p_display), False,
			   SubstructureNotifyMask | SubstructureRedirectMask, &event);
	XFlush(p_display);

	if(info.info.x11.unlock_func) info.info.x11.unlock_func();
	return true;
#else
	(void)wantFullScreen;
	return false;
#endif
}

bool setFixedSize(int width, int height)
{
#ifdef SDL_VIDEO_DRIVER_X11
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if(!SDL_GetWMInfo(&info) || info.subsystem != SDL_SYSWM_X11 || !info.info.x11.display) return false;

	if(info.info.x11.lock_func) info.info.x11.lock_func();

	// Equal minimum and maximum: no more is needed, that is the ICCCM way of
	// saying this window has one size. A window manager then locks the border
	// and the maximize button. The memory comes from Xlib and has to go back
	// there too.
	XSizeHints* p_hints = XAllocSizeHints();
	if(p_hints)
	{
		p_hints->flags      = PMinSize | PMaxSize;
		p_hints->min_width  = p_hints->max_width  = width;
		p_hints->min_height = p_hints->max_height = height;
		XSetWMNormalHints(info.info.x11.display, info.info.x11.window, p_hints);
		XFree(p_hints);
		XFlush(info.info.x11.display);
	}

	if(info.info.x11.unlock_func) info.info.x11.unlock_func();
	return p_hints != 0;
#else
	(void)width; (void)height;
	return false;
#endif
}

}
