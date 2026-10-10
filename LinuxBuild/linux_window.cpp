// linux_window.cpp - the fullscreen switch under X11, and an X event's time.
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
	// differently under each one. The message names the window the manager
	// manages: the classic SDL 1.2 draws into a child of it, which the manager
	// would ignore, while sdl12-compat reports the one window in both fields.
	XEvent event;
	memset(&event, 0, sizeof(event));
	event.type                 = ClientMessage;
	event.xclient.window       = info.info.x11.wmwindow ? info.info.x11.wmwindow : info.info.x11.window;
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

XEventKind readInputEvent(const SDL_Event& event, unsigned int* p_keycode, unsigned long* p_time)
{
#ifdef SDL_VIDEO_DRIVER_X11
	if(event.type != SDL_SYSWMEVENT || !event.syswm.msg || event.syswm.msg->subsystem != SDL_SYSWM_X11) return XE_NONE;

	const XEvent& x = event.syswm.msg->event.xevent;
	switch(x.type)
	{
	case KeyPress:
	case KeyRelease:
		*p_keycode = x.xkey.keycode;
		*p_time = x.xkey.time;
		return x.type == KeyPress ? XE_KEY_PRESS : XE_KEY_RELEASE;
	case ButtonPress:
	case ButtonRelease:
		*p_time = x.xbutton.time;
		return XE_POINTER;
	case MotionNotify:
		*p_time = x.xmotion.time;
		return XE_POINTER;
	case EnterNotify:
	case LeaveNotify:
		*p_time = x.xcrossing.time;
		return XE_POINTER;
	case FocusOut:
		return XE_FOCUS_OUT;
	default:
		return XE_NONE;
	}
#else
	(void)event;
	(void)p_keycode;
	(void)p_time;
	return XE_NONE;
#endif
}

}
