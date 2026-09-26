#include "pch.h"
#include "testhooks.h"

// test_hooks.cpp - how the test dump gets out in the browser: as a string in
// Module["b5_test"] or Module["b5_hit"] for Playwright (test/README.md). What
// is reported is Blocks5/src/testhooks.cpp's, the same on both platforms.
// Only ./build.sh hooks builds it; without -DBLOCKS5_TEST_HOOKS the
// translation unit is empty.

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

	// Where a measurement begins: the dump does not clear the frame timings,
	// because the -perf overlay reads the same numbers all the time.
	EMSCRIPTEN_KEEPALIVE void blocks5_testResetStats(void)
	{
		TestHooks::resetStats();
	}

	// A call of its own, not a field of the dump: a toggle's containsPoint()
	// measures its caption, and a hit test per element over every element is
	// forty thousand measurements at two hundred elements.
	EMSCRIPTEN_KEEPALIVE void blocks5_testHitAt(int x, int y)
	{
		EM_ASM({ Module["b5_hit"] = UTF8ToString($0); }, TestHooks::hitAt(x, y).c_str());
	}
}

#endif
