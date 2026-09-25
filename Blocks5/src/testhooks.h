#ifndef _TESTHOOKS_H
#define _TESTHOOKS_H

/*** Reporting point for the test harness's remote control ***/

// The GUI tree with every element's window rectangle, so that a test clicks
// on a name ("Menu.Options") rather than on a coordinate read off a
// screenshot; the click itself stays an ordinary mouse click through SDL,
// Engine and GUI. Besides the report, the native requests can stop the
// clock, switch the game state and press a button (see pollRequests()).
//
// Compiled only with -DBLOCKS5_TEST_HOOKS; the shipped build contains none
// of it. In the browser JavaScript fetches the answer
// (WebBuild/test_hooks.cpp); natively it goes through a file.

#ifdef BLOCKS5_TEST_HOOKS

namespace TestHooks
{
	// The whole GUI tree as JSON.
	std::string dump();

	// Which element a click on this point would reach: how a test sees that
	// something else lies on top of its target.
	std::string hitAt(int x, int y);

	// Clear the frame timings and the draw and cache counters. The dump
	// reports them without clearing, because the -perf overlay reads the same
	// numbers, so a measurement says here where it begins.
	void resetStats();

	// Stop the logic clock at a named tick of Engine::sceneTick. The seeded
	// generator is keyed on that tick, while how many ticks a machine runs
	// per rendered frame depends on its speed, so a named tick is what makes
	// a frame comparable between two builds. checkFreeze() runs before each
	// tick, so the clock stops exactly on the tick asked for.
	void freezeAt(uint tick);
	// The same for a crossfade: stop at the first tick at which the running
	// crossfade has reached ms milliseconds, whatever screen clock stands.
	void freezeAtFade(uint ms);
	// fadeMs is the running crossfade's progress in milliseconds, or -1
	// where none runs.
	void checkFreeze(uint tick, int fadeMs);
	bool frozen();
	// True exactly once after the clock stops: the main loop then renders
	// the frozen frame once more with getTime() pinned to zero,
	// so the picture does not depend on the harness's timing.
	bool frozenFrameDue();

	// One logic tick per rendered frame, for a screen whose picture depends
	// on how many frames were rendered rather than on how many ticks ran.
	void setLockstep(bool on);
	bool lockstep();

	// Every draw call the game issues, counted natively by the wrappers at
	// the foot of testhooks.cpp; Engine::render() reads it before and after.
	// In the browser nothing feeds it: WebBuild/test/harness.js counts on
	// the WebGL context instead.
	extern uint drawCalls;

#ifndef __EMSCRIPTEN__
	// Answers a request waiting in $B5_TEST_DIR. Called once per logic tick
	// from Engine::update(), and in the tick's place while the clock is
	// frozen.
	void pollRequests();
#endif
}

#endif // BLOCKS5_TEST_HOOKS

#endif
