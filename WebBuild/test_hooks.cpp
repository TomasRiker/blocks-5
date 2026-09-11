#include "pch.h"
#include "testhooks.h"

// test_hooks.cpp - the dump's way back into the browser.
//
// What is reported lives in Blocks5/src/testhooks.cpp and is the same on both
// platforms. This file says only how the text gets out: as a string in
// Module["b5_test"] or Module["b5_hit"], where Playwright picks it up (see
// test/README.md).
//
// This is built by ./build.sh hooks alone; without -DBLOCKS5_TEST_HOOKS the
// translation unit is empty and the shipped build holds none of it.

#if defined(__EMSCRIPTEN__) && defined(BLOCKS5_TEST_HOOKS)

#include <emscripten.h>

extern "C"
{
	// The way back through a C string would need ccall/UTF8ToString among the
	// exported runtime methods; inside an EM_ASM body UTF8ToString is there
	// anyway.
	EMSCRIPTEN_KEEPALIVE void blocks5_testDump(void)
	{
		const std::string json = TestHooks::dump();
		EM_ASM({ Module["b5_test"] = UTF8ToString($0); }, json.c_str());
	}

	// Where a measurement begins. The dump reports the frame timings without
	// clearing them, since the -perf overlay reads the same numbers all the
	// time; a test therefore says when its window opens instead of letting
	// the reading decide.
	EMSCRIPTEN_KEEPALIVE void blocks5_testResetStats(void)
	{
		TestHooks::resetStats();
	}

	// On its own and not as a field in the dump: for a toggle containsPoint()
	// measures the width of its caption, and doing that per element for every
	// element would be forty thousand measurements at two hundred elements.
	EMSCRIPTEN_KEEPALIVE void blocks5_testHitAt(int x, int y)
	{
		EM_ASM({ Module["b5_hit"] = UTF8ToString($0); }, TestHooks::hitAt(x, y).c_str());
	}
}

#endif
