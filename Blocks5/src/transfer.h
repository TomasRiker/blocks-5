#ifndef _TRANSFER_H
#define _TRANSFER_H

/*** Dateien zwischen dem Spiel und der Aussenwelt bewegen ***/

// Alles hier gibt es auf beiden Plattformen. Der Unterschied steckt nur im
// Dateidialog: unter Windows ist er modal und liefert einen Pfad, den das
// Spiel direkt lesen darf; im Browser ist er asynchron und legt eine Kopie in
// eine Zwischendatei. beginImport()/pollImport() verdecken das.

class Campaign;

namespace Transfer
{
	enum Kind
	{
		KIND_NONE = 0,
		KIND_LEVEL,
		KIND_CAMPAIGN,
		KIND_MUSIC,
		KIND_SKIN
	};

	// Erkennt die Art am Inhalt, nicht an der Endung: Musik am
	// OggS-Kennzeichen, ein Level an seiner XML-Wurzel, ein Archiv daran, ob
	// eine campaign.xml oder ein tileset.xml darin liegt.
	Kind classify(const std::string& path);

	// Uebernimmt eine Datei ins Benutzerverzeichnis und liefert den vergebenen
	// Dateinamen, oder "" und setzt dann errorId. untrustedName ist nur ein
	// Vorschlag; der Zielpfad entsteht hier. Eine gleichnamige Datei wird
	// ersetzt, was p_replaced meldet; die Namen aus isBuiltIn() nicht.
	std::string install(Kind kind,
						const std::string& path,
						const std::string& untrustedName,
						std::string& errorId,
						bool* p_replaced = 0);

	// Was von dieser Art im Benutzerverzeichnis liegt, alphabetisch sortiert.
	std::vector<std::string> list(Kind kind);

	// Gehoert diese Datei zum Spiel? name ist der Dateiname mit Endung, so wie
	// list() ihn liefert. Ein Import darf einen solchen Namen nicht besetzen
	// und der Manager ihn nicht loeschen.
	bool isBuiltIn(Kind kind, const std::string& name);

	// Legt die mitgelieferten Dateien neu ins Benutzerverzeichnis, wo sie fehlen
	// oder sich von den ausgelieferten unterscheiden. Ohne das wird der Ordner
	// genau einmal befuellt - bei der allerersten Installation - und eine
	// spaetere Aenderung an einem Skin, an der Kampagne oder an einem
	// Beispiellevel erreicht ein vorhandenes Spiel nie.
	bool refreshBuiltIns();

	// Loescht, was list() geliefert hat, ausser den mitgelieferten Dateien.
	bool remove(Kind kind, const std::string& name, std::string& errorId);

	enum Status
	{
		STATUS_BUSY = 0,
		STATUS_OK,
		STATUS_CANCELLED,
		STATUS_TOO_BIG,
		STATUS_UNKNOWN,
		STATUS_FAILED
	};

	// false heisst "gerade nicht moeglich, bitte noch einmal klicken": der
	// Browser gibt einen Dateidialog nur her, solange er einen Klick des
	// Benutzers als frisch ansieht.
	bool beginImport();

	// Jeden Logiktakt aufrufen. Bei STATUS_OK stehen in path eine lesbare
	// Datei und in untrustedName der Wunschname von aussen.
	int pollImport(std::string& path, std::string& untrustedName);

	// Nach STATUS_OK aufrufen, wenn die Datei verarbeitet ist.
	void finishImport();

	// Beim Verlassen des Menues: verwirft einen noch offenen Dialog.
	void abandonImport();

	// Modal auf beiden Seiten. false heisst abgebrochen oder gescheitert;
	// wurde nur abgebrochen, bleibt errorId leer.
	bool doExport(Kind kind, const std::string& name, std::string& errorId);
}

#endif
