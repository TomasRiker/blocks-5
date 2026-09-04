#ifndef _LINUX_WINDOW_H
#define _LINUX_WINDOW_H

/*** Was am Fenster nur ueber X11 geht ***/

// Diese Kopfdatei zieht mit Absicht kein <X11/Xlib.h> herein: Xlib belegt
// Font, Window, Screen und Cursor als eigene Typnamen, und die Klassen des
// Spiels heissen genauso. Alles Xlib-Eigene bleibt deshalb in linux_window.cpp.

namespace LinuxWindow
{
	// Den Fenstermanager bitten, das Spielfenster ins Vollbild zu nehmen oder
	// wieder herauszuholen. false heisst "hier laeuft kein X11" - dann muss
	// der Aufrufer selbst sehen, was er tut.
	bool setFullScreen(bool wantFullScreen);
}

#endif
