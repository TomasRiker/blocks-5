#include "pch.h"
#include "testhooks.h"

// test_hooks.cpp - der Rueckweg der Auskunftsstelle in den Browser.
//
// Was berichtet wird, steht in Blocks5/src/testhooks.cpp und ist auf beiden
// Plattformen dasselbe. Hier steht nur, wie der Text nach draussen kommt: als
// Zeichenkette in Module["b5_test"] bzw. Module["b5_hit"], wo Playwright sie
// abholt (siehe test/README.md).
//
// Gebaut wird das nur mit ./build.sh hooks; ohne -DBLOCKS5_TEST_HOOKS ist die
// Uebersetzungseinheit leer, und der ausgelieferte Build enthaelt nichts davon.

#if defined(__EMSCRIPTEN__) && defined(BLOCKS5_TEST_HOOKS)

#include <emscripten.h>

extern "C"
{
	// Der Rueckweg ueber eine C-Zeichenkette braeuchte ccall/UTF8ToString unter
	// den exportierten Laufzeitmethoden; im EM_ASM-Rumpf ist UTF8ToString
	// ohnehin da.
	EMSCRIPTEN_KEEPALIVE void blocks5_testDump(void)
	{
		const std::string json = TestHooks::dump();
		EM_ASM({ Module["b5_test"] = UTF8ToString($0); }, json.c_str());
	}

	// Einzeln und nicht als Feld im Bericht: containsPoint() misst bei einem
	// Schalter die Breite seiner Beschriftung, und das je Element fuer jedes
	// Element waeren bei zweihundert Elementen vierzigtausend Messungen.
	EMSCRIPTEN_KEEPALIVE void blocks5_testHitAt(int x, int y)
	{
		EM_ASM({ Module["b5_hit"] = UTF8ToString($0); }, TestHooks::hitAt(x, y).c_str());
	}
}

#endif
