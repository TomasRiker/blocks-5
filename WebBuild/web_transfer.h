#ifndef _WEB_TRANSFER_H
#define _WEB_TRANSFER_H
#ifdef __EMSCRIPTEN__

/*** Datei-Austausch zwischen dem virtuellen Dateisystem und dem Browser ***/

#include <string>

namespace WebTransfer
{
	enum ImportStatus
	{
		IMPORT_IDLE       = 0,   // nichts passiert
		IMPORT_OK         = 1,   // Datei liegt unter stagingPath, Name im Puffer
		IMPORT_CANCELLED  = 2,   // Benutzer hat abgebrochen
		IMPORT_TOO_BIG    = 3,
		IMPORT_WRONG_TYPE = 4,
		IMPORT_READ_ERROR = 5
	};

	// Laedt den Inhalt von vfsPath als Download herunter.
	void download(const std::string& vfsPath, const std::string& downloadName);

	// Wie download(), aber fuer Daten, die noch in keiner Datei stehen.
	bool downloadString(const std::string& data, const std::string& downloadName);

	// Oeffnet den Dateidialog. acceptExtension ist ".xml" oder ".zip";
	// stagingPath MUSS mit derselben Erweiterung enden (siehe
	// FileSystem::convertPath - Archive werden am ".zip/" erkannt) und darf
	// NICHT unter dem Home-Verzeichnis liegen, damit eine abgelehnte Datei
	// gar nicht erst in IndexedDB landet.
	// Liefert false, wenn schon ein Dialog laeuft oder der Browser gerade
	// keine Benutzer-Aktivierung sieht.
	// channel unterscheidet die Stellen, die einen Dialog oeffnen koennen: ein
	// Dialog, der erst aufgeloest wird, nachdem der Benutzer den Bildschirm
	// gewechselt hat, darf nicht anderswo eingesammelt und als kaputte Datei
	// gemeldet werden. Der Level-Editor hat zwei davon, weil er Level und
	// Skins holen kann, und Kampagnen kommen aus dem Kampagnen-Editor oder aus
	// der Levelauswahl.
	enum
	{
		CHANNEL_LEVEL        = 1,
		CHANNEL_CAMPAIGN     = 2,
		CHANNEL_SKIN         = 3,
		CHANNEL_SELECT_LEVEL = 4
	};

	bool openPicker(int channel,
	                const std::string& acceptExtension,
	                unsigned int maxBytes,
	                const std::string& stagingPath);

	// Einmal pro Logik-Tick aufrufen. Liefert IMPORT_IDLE, solange nichts
	// fertig ist oder das Ergebnis einem anderen Kanal gehoert; sonst den
	// Status und den (ungeprueften!) Wunschnamen.
	int pollImport(int channel, std::string& untrustedName);

	// Beim Verlassen eines Editors aufrufen: verwirft einen noch offenen
	// Dialog dieses Kanals und raeumt die Zwischendatei weg.
	void abandon(int channel, const std::string& stagingPath);

	// Erzwingt ein FS.syncfs, damit ein Import sofort in IndexedDB landet.
	void syncHome();
}

#endif
#endif