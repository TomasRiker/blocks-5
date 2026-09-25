// linux_window.cpp - the fullscreen switch under X11.
//
// Its own translation unit because <X11/Xlib.h>, which SDL_syswm.h brings in,
// declares a Font type of its own that collides with the game's Font class.
#include <SDL.h>
#include <SDL_syswm.h>
#include <cstring>
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

	// SDL may pump X events on a thread of its own, which these lock
	// (SDL_syswm.h); every Xlib call on its display belongs between the two.
	if(info.info.x11.lock_func) info.info.x11.lock_func();

	// Under X11 the window manager decides on fullscreen, size and position;
	// the program asks with this EWMH message to the root window.
	// XMoveResizeWindow would go past the window manager and behave
	// differently under each one.
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

}
