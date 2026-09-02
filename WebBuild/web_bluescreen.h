#ifndef _WEB_BLUESCREEN_H
#define _WEB_BLUESCREEN_H

/* Ein Osterei fuer die Browserfassung.

   "Beenden" kann dort nichts beenden - ein Programm schliesst seinen eigenen
   Tab nicht. Bisher passierte auf den Knopf hin also schlicht gar nichts, was
   sich wie ein Fehler anfuehlt. Statt dessen tut das Spiel jetzt so, als haette
   es den Rechner mitgerissen.

   Nur im Emscripten-Build; unter Windows beendet SDL_QUIT das Spiel wie immer. */

namespace WebBlueScreen
{
	// Blendet den blauen Schirm ein und haelt die Hauptschleife an. Ein
	// Tastendruck oder Klick laedt die Seite neu - das ist der Neustart, von dem
	// der Text spricht.
	void show();
}

#endif
