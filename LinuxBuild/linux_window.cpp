// linux_window.cpp - der Vollbildwechsel unter X11.
//
// Eigene Uebersetzungseinheit, weil <X11/Xlib.h> hier hereinkommt: es macht
// Font, Window, Screen und Cursor zu eigenen Typnamen, und engine.cpp benutzt
// gleich hinter der Einbindung die Klassen des Spiels, die genauso heissen.
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

	// SDL zeichnet aus einem eigenen Faden; jeder Xlib-Aufruf von aussen
	// gehoert zwischen diese beiden.
	if(info.info.x11.lock_func) info.info.x11.lock_func();

	// Ein Programm setzt sein Fenster unter X11 nicht selbst auf Vollbild - es
	// sagt dem Fenstermanager, dass es eines haben moechte, und der entscheidet
	// ueber Groesse und Ort. Der Weg dafuer ist diese Nachricht an das
	// Wurzelfenster, so beschrieben in der EWMH; jeder Fenstermanager der
	// letzten zwanzig Jahre versteht sie. XMoveResizeWindow statt dessen ginge
	// an ihm vorbei und liefe unter jedem einzelnen anders.
	XEvent event;
	memset(&event, 0, sizeof(event));
	event.type                 = ClientMessage;
	event.xclient.window       = info.info.x11.window;
	event.xclient.message_type = XInternAtom(p_display, "_NET_WM_STATE", False);
	event.xclient.format       = 32;
	event.xclient.data.l[0]    = wantFullScreen ? 1 : 0;   // _NET_WM_STATE_ADD / _REMOVE
	event.xclient.data.l[1]    = XInternAtom(p_display, "_NET_WM_STATE_FULLSCREEN", False);
	event.xclient.data.l[2]    = 0;
	event.xclient.data.l[3]    = 1;                        // von der Anwendung, nicht von einem Pager
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
