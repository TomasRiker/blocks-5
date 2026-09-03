#ifndef _TESTHOOKS_H
#define _TESTHOOKS_H

/*** Auskunftsstelle fuer die Fernsteuerung beim Testen ***/

// Die Oberflaeche kennt von aussen nur Pixel: wer einen Knopf treffen will,
// muss seine Bildschirmkoordinate raten und aus einem Bildschirmfoto ablesen.
// Das geht regelmaessig daneben - der Knopf ist 18 Pixel hoch, das Fenster ist
// skaliert, und ob wirklich er getroffen wurde oder das Element darunter,
// sieht man dem Foto nicht an.
//
// Hier steht statt dessen der GUI-Baum mit den Fensterkoordinaten jedes
// Elements. Der Test klickt dann auf einen Namen ("Menu.Options") und nicht
// auf eine Zahl, und der Klick selbst bleibt ein gewoehnlicher Mausklick und
// geht denselben Weg durch SDL, Engine und GUI wie im Spiel.
//
// Alles hier liest nur. Uebersetzt wird es nur mit -DBLOCKS5_TEST_HOOKS; ohne
// das ist die Uebersetzungseinheit leer und der ausgelieferte Build enthaelt
// nichts davon. Wie die Antwort nach draussen kommt, ist Sache der Plattform:
// im Browser holt sie JavaScript ab (WebBuild/test_hooks.cpp), unter Linux
// liegt sie in einer Datei (siehe pollRequests()).

#ifdef BLOCKS5_TEST_HOOKS

namespace TestHooks
{
	// Der ganze GUI-Baum als JSON.
	std::string dump();

	// Wer bekaeme einen Klick auf diesen Punkt? Beantwortet die Frage, an der
	// ein Test sonst scheitert: liegt etwas anderes darueber?
	std::string hitAt(int x, int y);

#ifndef __EMSCRIPTEN__
	// Einmal je Logiktakt aus Engine::update(). Liegt eine Anfrage im
	// Testverzeichnis, wird sie beantwortet.
	void pollRequests();
#endif
}

#endif // BLOCKS5_TEST_HOOKS

#endif
