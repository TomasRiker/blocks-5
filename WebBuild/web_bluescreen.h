#ifndef _WEB_BLUESCREEN_H
#define _WEB_BLUESCREEN_H

/* An easter egg for the browser version.

   "Quit" cannot quit anything there - a program does not close its own tab,
   and a button that simply does nothing feels like a fault. The game pretends
   instead that it has taken the machine down with it.

   Emscripten build only; under Windows SDL_QUIT quits the game as always. */

namespace WebBlueScreen
{
	// Brings the blue screen up and stops the main loop. A key press or a
	// click reloads the page - that is the restart the text asks for.
	void show();
}

#endif
