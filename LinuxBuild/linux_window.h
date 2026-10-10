#ifndef _LINUX_WINDOW_H
#define _LINUX_WINDOW_H

/*** What can only be done or read through X11 ***/

// No <X11/Xlib.h> here: it declares a Font type of its own, which collides
// with the game's Font class, so Xlib stays in linux_window.cpp.

#include <SDL.h>

namespace LinuxWindow
{
	// Ask the window manager to take the game window into fullscreen or back
	// out. false means no X11 here, and the caller is on its own.
	bool setFullScreen(bool wantFullScreen);

	// What an SDL_SYSWMEVENT says of the X event it carries, for the time of
	// the key and mouse events SDL makes of that event (inputtime.cpp): the
	// kind, a key's keycode and the time on the X server's clock, which a
	// focus going carries none of. XE_NONE for any other event.
	enum XEventKind
	{
		XE_NONE,
		XE_KEY_PRESS,
		XE_KEY_RELEASE,
		XE_POINTER,
		XE_FOCUS_OUT
	};
	XEventKind readInputEvent(const SDL_Event& event, unsigned int* p_keycode, unsigned long* p_time);
}

#endif
