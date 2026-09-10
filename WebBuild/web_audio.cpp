#include "pch.h"
#ifdef __EMSCRIPTEN__
#include "web_audio.h"
#include <emscripten.h>

// Careful: EM_ASM bodies run through the C preprocessor. A single apostrophe
// would be a broken character literal there; use nothing but double quotes in
// the JS.

// AL is the library object out of libopenal.js. It sits in the same module
// scope as the embedded EM_ASM bodies and the minifier does not rename it
// (library objects are top-level names). Everything is in a try/catch all the
// same: where the access fails, the game reports "not blocked" and carries on
// without a gatekeeper - silent rather than hung.

namespace WebAudio
{

bool isSuspended()
{
	return EM_ASM_INT({
		try {
			var ctx = AL.currentCtx && AL.currentCtx.audioCtx;
			return (ctx && ctx.state === "suspended") ? 1 : 0;
		} catch (e) { return 0; }
	}) != 0;
}

void resume()
{
	EM_ASM({
		try {
			var ctx = AL.currentCtx && AL.currentCtx.audioCtx;
			// resume() returns a promise; a failure must not land in the
			// console as an unhandled rejection.
			if (ctx && ctx.state === "suspended") ctx.resume().catch(function() {});
		} catch (e) {}
	});
}

}

#endif
