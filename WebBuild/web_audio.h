#ifndef _WEB_AUDIO_H
#define _WEB_AUDIO_H

#ifdef __EMSCRIPTEN__

/*** Access to the browser's AudioContext ***

	A browser starts an AudioContext created without a user gesture in the
	"suspended" state: everything the game plays before the first click is
	lost. Emscripten hangs a waker off the first mousedown/keydown/touchstart
	itself (autoResumeAudioContext in libcore.js), but the loading screen
	needs to know all the same - it has to wait before the jingle runs.
*/

namespace WebAudio
{
	// true while the browser blocks the audio output. Where there is no
	// context at all (no audio device, OpenAL init failed) the answer is
	// false: then there is nothing to unblock either.
	bool isSuspended();

	// Wakes the context. Must be called from within a user gesture, or it
	// has no effect.
	void resume();
}

#endif

#endif
