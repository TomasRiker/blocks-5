#ifndef _WEB_AUDIO_H
#define _WEB_AUDIO_H

#ifdef __EMSCRIPTEN__

/*** Zugriff auf den AudioContext des Browsers ***

	Browser starten einen AudioContext, der ohne Benutzergeste angelegt wurde,
	im Zustand "suspended": alles, was das Spiel bis zum ersten Klick abspielt,
	ist verloren. Emscripten haengt zwar selbst einen Aufwecker an das erste
	mousedown/keydown/touchstart (autoResumeAudioContext in libcore.js), aber
	der Ladebildschirm braucht die Information trotzdem - er muss warten,
	bevor der Jingle laeuft.
*/

namespace WebAudio
{
	// true, solange der Browser die Tonausgabe blockiert. Gibt es gar keinen
	// Kontext (kein Audiogeraet, OpenAL-Init fehlgeschlagen), lautet die
	// Antwort false: dann gibt es auch nichts freizuschalten.
	bool isSuspended();

	// Weckt den Kontext. Muss aus einer Benutzergeste heraus aufgerufen
	// werden, sonst bleibt es wirkungslos.
	void resume();
}

#endif

#endif
