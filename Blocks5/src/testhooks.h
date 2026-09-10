#ifndef _TESTHOOKS_H
#define _TESTHOOKS_H

/*** Reporting point for the test harness's remote control ***/

// From outside the GUI is nothing but pixels: hitting a button means
// guessing its screen coordinate and reading it off a screenshot. That goes
// wrong regularly - the button is 18 pixels high, the window is scaled, and
// whether the button or the element underneath took the click cannot be told
// from the picture.
//
// Here instead is the GUI tree, with the window coordinates of every
// element. The test then clicks on a name ("Menu.Options") and not on a
// number, and the click itself stays an ordinary mouse click travelling the
// same way through SDL, Engine and GUI as in the game.
//
// Everything here only reads. It is compiled only with -DBLOCKS5_TEST_HOOKS;
// without that the translation unit is empty and the shipped build contains
// none of it. How the answer gets out is the platform's business: in the
// browser JavaScript fetches it (WebBuild/test_hooks.cpp), under Linux it
// sits in a file (see pollRequests()).

#ifdef BLOCKS5_TEST_HOOKS

namespace TestHooks
{
	// The whole GUI tree as JSON.
	std::string dump();

	// Which element would a click on this point reach? It answers the
	// question a test otherwise fails on: is something else lying on top?
	std::string hitAt(int x, int y);

	// Throw away the frame timings and start again. The dump reports them
	// without clearing, because the -perf overlay reads the same numbers
	// continuously; a measurement therefore says where it begins rather than
	// having the reading define it.
	void resetStats();

#ifndef __EMSCRIPTEN__
	// Once per logic tick from Engine::update(). If a request is sitting in
	// the test directory, it is answered.
	void pollRequests();
#endif
}

#endif // BLOCKS5_TEST_HOOKS

#endif
