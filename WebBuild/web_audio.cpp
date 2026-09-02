#include "pch.h"
#ifdef __EMSCRIPTEN__
#include "web_audio.h"
#include <emscripten.h>

// Achtung: EM_ASM-Rumpfe laufen durch den C-Praeprozessor. Ein einzelnes
// Apostroph waere dort ein kaputtes Zeichenliteral - im JS also ausschliesslich
// doppelte Anfuehrungszeichen benutzen.

// AL ist das Bibliotheksobjekt aus libopenal.js. Es liegt im selben
// Modul-Gueltigkeitsbereich wie die eingebetteten EM_ASM-Rumpfe und wird vom
// Minifier nicht umbenannt (Bibliotheksobjekte sind Namen auf oberster Ebene).
// Trotzdem alles in try/catch: faellt der Zugriff aus, meldet das Spiel
// "nicht blockiert" und laeuft ohne Torwaechter weiter - lieber stumm als
// haengend.

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
			// resume() liefert ein Promise; ein Fehlschlag darf nicht als
			// unbehandelte Ablehnung in der Konsole landen.
			if (ctx && ctx.state === "suspended") ctx.resume().catch(function() {});
		} catch (e) {}
	});
}

}

#endif
