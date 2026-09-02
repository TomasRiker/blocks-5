#ifndef _TRANSFER_H
#define _TRANSFER_H

/*** Dateien zwischen dem Spiel und der Aussenwelt bewegen ***/

// Frueher hatte jeder Editor seine eigenen Export- und Import-Knoepfe, an die
// Stelle gesetzt, wo im Fenster gerade Platz war. Das hatte drei Nachteile:
// man musste einen Editor oeffnen, um eine fremde Kampagne zu installieren,
// fuer Musik gab es ueberhaupt keinen Weg, und ein Skin liess sich nur dort
// holen, wo man ihn am wenigsten braucht. Jetzt gibt es dafuer einen einzigen
// Knopf im Hauptmenue, der einen Dialog aufmacht: einspielen, ausgeben,
// loeschen.
//
// Zusammen gehoeren die drei, weil jeder allein eine Haelfte fehlt. Der
// Import hatte gar keinen Dialog - er nahm eine Datei an, sagte in einer
// Meldung, was daraus geworden war, und der Spieler sah nie die Liste, in die
// sie gewandert war. Der Export hatte nichts als diese Liste. Und geloescht
// werden konnte gar nichts: im Browser gibt es keinen Dateimanager daneben,
// mit dem man es haette nachholen koennen.
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

	// Gehoert diese Datei zum Spiel? name ist der Dateiname mit Endung, so wie
	// list() ihn liefert. Sieben Stueck sind es: die mitgelieferte Kampagne,
	// ihre vier Skins und die beiden Beispiellevel. Musik ist nie dabei - die
	// zehn Stuecke stecken in blocks.zip und liegen nicht einzeln herum.
	//
	// Zwei Stellen fragen danach, und zwar aus demselben Grund: der Manager
	// darf eine solche Datei nicht loeschen, und ein Import darf ihren Namen
	// nicht besetzen. Wer skin0="space" in einem Level stehen hat, meint
	// levels/skins/space.zip und keine andere Datei.
	bool isBuiltIn(Kind kind, const std::string& name);

	// Loescht, was list() geliefert hat. Verweigert die mitgelieferten
	// Dateien. false setzt errorId auf die anzuzeigende Meldung.
	bool remove(Kind kind, const std::string& name, std::string& errorId);

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
