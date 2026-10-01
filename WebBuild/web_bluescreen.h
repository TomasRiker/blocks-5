#ifndef _WEB_BLUESCREEN_H
#define _WEB_BLUESCREEN_H

/* An easter egg for the browser version. A page cannot close its own tab,
   and a Quit button that does nothing feels like a fault, so the game
   pretends instead that it has taken the machine down with it.

   Emscripten build only; on the desktop SDL_QUIT quits the game. */

namespace WebBlueScreen
{
	// Brings the blue screen up and stops the main loop. A key press, a
	// click or a touch reloads the page - that is the restart the text asks
	// for.
	void show();
}

#endif
