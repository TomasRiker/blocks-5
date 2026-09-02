#ifndef _WEB_TRANSFER_H
#define _WEB_TRANSFER_H
#ifdef __EMSCRIPTEN__

/*** Datei-Austausch zwischen dem virtuellen Dateisystem und dem Browser ***/

// Die Brueckenschicht unter Transfer (src/transfer.h): sie kennt nur Blobs,
// Dateidialoge und IndexedDB, nichts von Levels, Kampagnen, Musik und Skins.

#include <string>

namespace WebTransfer
{
	enum ImportStatus
	{
		IMPORT_IDLE       = 0,   // nichts passiert
		IMPORT_OK         = 1,   // Datei liegt in einer der drei Zwischendateien
		IMPORT_CANCELLED  = 2,   // Benutzer hat abgebrochen
		IMPORT_TOO_BIG    = 3,
		IMPORT_WRONG_TYPE = 4,   // Endung ist keine der drei
		IMPORT_READ_ERROR = 5
	};

	// Laedt den Inhalt von vfsPath als Download herunter.
	void download(const std::string& vfsPath, const std::string& downloadName);

	// Oeffnet den Dateidialog. Der Aufrufer gibt alle drei moeglichen Ziele
	// vor und JS sucht sich nach der Endung eines davon aus - so setzt
	// weiterhin ausschliesslich C einen Pfad zusammen. Alle drei muessen auf
	// die jeweilige Endung enden (FileSystem::convertPath erkennt ein Archiv
	// am ".zip/") und duerfen NICHT unter dem Home-Verzeichnis liegen, damit
	// eine abgelehnte Datei gar nicht erst in die IndexedDB kommt.
	// Liefert false, wenn schon ein Dialog laeuft oder der Browser gerade
	// keine Benutzer-Aktivierung sieht.
	bool openPicker(const std::string& stagingOgg,
	                const std::string& stagingXml,
	                const std::string& stagingZip,
	                unsigned int maxBytes);

	// Einmal pro Logik-Tick aufrufen. Liefert IMPORT_IDLE, solange nichts
	// fertig ist; sonst den Status und den (ungeprueften!) Wunschnamen.
	int pollImport(std::string& untrustedName);

	// Verwirft einen noch offenen Dialog. Die Zwischendateien raeumt der
	// Aufrufer selbst weg - er hat sie schliesslich benannt.
	void abandon();

	// Erzwingt ein FS.syncfs, damit ein Import sofort in IndexedDB landet.
	void syncHome();
}

#endif
#endif
