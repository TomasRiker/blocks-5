#ifndef _LINUX_WINDOW_H
#define _LINUX_WINDOW_H

/*** What can only be done to the window through X11 ***/

// No <X11/Xlib.h> here: it declares a Font type of its own, which collides
// with the game's Font class, so Xlib stays in linux_window.cpp.

namespace LinuxWindow
{
	// Ask the window manager to take the game window into fullscreen or back
	// out. false means no X11 here, and the caller is on its own.
	bool setFullScreen(bool wantFullScreen);
}

#endif
