#ifndef _LINUX_WINDOW_H
#define _LINUX_WINDOW_H

/*** What can only be done to the window through X11 ***/

// This header deliberately pulls in no <X11/Xlib.h>: Xlib takes Font, Window,
// Screen and Cursor as type names of its own, and the game's classes are
// called the same. Everything Xlib-specific therefore stays in
// linux_window.cpp.

namespace LinuxWindow
{
	// Ask the window manager to take the game window into fullscreen or to
	// bring it back out. false means "no X11 running here" - the caller is then
	// on its own.
	bool setFullScreen(bool wantFullScreen);

	// Tell the window manager that this window is to have exactly this size and
	// no other. The manager then takes the drag handle off the border and the
	// effect off the maximize button. false again means "no X11 running here".
	bool setFixedSize(int width, int height);
}

#endif
