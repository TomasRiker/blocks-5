#ifndef _TRANSFER_H
#define _TRANSFER_H

/*** Dateien zwischen dem Spiel und der Aussenwelt bewegen ***/

// Frueher hatte jeder Editor seine eigenen Export- und Import-Knoepfe, an die
// Stelle gesetzt, wo im Fenster gerade Platz war. Das hatte drei Nachteile:
// man musste einen Editor oeffnen, um eine fremde Kampagne zu installieren,
// fuer Musik gab es ueberhaupt keinen Weg, und ein Skin liess sich nur dort
// holen, wo man ihn am wenigsten braucht. Jetzt gibt es genau zwei Knoepfe,
// im Hauptmenue: einer nimmt eine Datei an und erkennt selbst, was sie ist,
// der andere fragt, was hinausgehen soll.
//
// Alles hier ist auf beiden Plattformen da. Der Unterschied steckt nur im
// Dateidialog: unter Windows ist er modal und liefert einen Pfad, den das
// Spiel direkt lesen darf; im Browser ist er asynchron und legt eine Kopie in
// eine Zwischendatei. beginImport()/pollImport() verdecken das.

class Campaign;

namespace Transfer
{
	// Was das Spiel annimmt und herausgibt.
	enum Kind
	{
		KIND_NONE = 0,
		KIND_LEVEL,
		KIND_CAMPAIGN,
		KIND_MUSIC,
		KIND_SKIN
	};

	// Was fuer eine Datei ist das? Liest nur und legt nichts an. Die vier
	// Arten sind an ihrem Inhalt zu unterscheiden, nicht an der Endung:
	// Musik am OggS-Kennzeichen, ein Level an seiner XML-Wurzel, ein Archiv
	// daran, ob eine campaign.xml oder ein tileset.xml darin liegt.
	Kind classify(const std::string& path);

	// Uebernimmt eine Datei ins Benutzerverzeichnis. untrustedName ist der
	// Name von aussen und nur ein Vorschlag - der Zielpfad entsteht hier.
	// Liefert den vergebenen Dateinamen, oder "" und setzt dann errorId auf
	// die anzuzeigende Meldung.
	std::string install(Kind kind,
						const std::string& path,
						const std::string& untrustedName,
						std::string& errorId);

	// Was von dieser Art im Benutzerverzeichnis liegt, alphabetisch sortiert.
	std::vector<std::string> list(Kind kind);

	// Der Name, den ein Export vorschlagen soll.
	std::string suggestedFilename(Kind kind, const std::string& name);

	// Schreibt eine Sache nach destPath. Ein passwortgeschuetzter Skin wird
	// dabei entschluesselt neu gepackt - sonst bekaeme der Empfaenger ein
	// Archiv, das kein Programm der Welt aufbekommt.
	bool exportTo(Kind kind, const std::string& name, const std::string& destPath);

	// --- der Dateidialog ---------------------------------------------------
	enum Status
	{
		STATUS_BUSY = 0,     // laeuft noch
		STATUS_OK,           // Datei liegt bereit
		STATUS_CANCELLED,
		STATUS_TOO_BIG,
		STATUS_UNKNOWN,      // keine Datei, mit der das Spiel etwas anfangen kann
		STATUS_FAILED
	};

	// Fragt nach einer Datei. false heisst "gerade nicht moeglich, bitte noch
	// einmal klicken" - der Browser gibt einen Dateidialog nur her, solange
	// er einen Klick des Benutzers als frisch ansieht.
	bool beginImport();

	// Jeden Logiktakt aufrufen. Bei STATUS_OK stehen in path eine lesbare
	// Datei und in untrustedName der Wunschname von aussen.
	int pollImport(std::string& path, std::string& untrustedName);

	// Nach STATUS_OK aufrufen, wenn die Datei verarbeitet ist: raeumt eine
	// etwaige Zwischendatei weg.
	void finishImport();

	// Beim Verlassen des Menues: verwirft einen noch offenen Dialog.
	void abandonImport();

	// Fragt nach einem Ziel und schreibt dorthin. Modal auf beiden Seiten;
	// liefert false, wenn abgebrochen wurde oder das Schreiben scheiterte.
	// Wurde nur abgebrochen, bleibt errorId leer.
	bool doExport(Kind kind, const std::string& name, std::string& errorId);
}

#endif
