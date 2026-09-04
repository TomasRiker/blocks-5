#ifndef _CAMPAIGN_H
#define _CAMPAIGN_H

/*** Klasse fuer eine Kampagne ***/

class Campaign
{
public:
	// Ein Level einer Kampagne. Die Identitaet ist das Paar (sourceDir,
	// member); "name" ist blosser Anzeigetext aus campaign.xml und wird nie zu
	// einem Pfad zusammengesetzt.
	struct LevelRef
	{
		LevelRef() : fromArchive(false) {}

		std::string source() const { return sourceDir + member; }

		std::string name;        // Text aus campaign.xml, nur zur Anzeige
		std::string sourceDir;   // "<home>levels/" oder "<kampagne>.zip[pw]/"
		std::string member;      // Dateiname bzw. Name des Archivmitglieds
		bool fromArchive;
	};

	// Verweis auf eine lose Datei im Level-Ordner des Benutzers.
	static LevelRef makeLooseRef(const std::string& filename);

	// Wohin ein musicFilename zeigt. sourceDir ist das Verzeichnis, in dem
	// gewoehnliche Stuecke des Levels liegen - "<home>levels/" fuer einen
	// losen Level, "<kampagne>.zip[pw]/" fuer einen aus einem Archiv. Faengt
	// der Name mit "blocks:" an, meint er ein Stueck der mitgelieferten
	// Kampagne und sourceDir ist ohne Belang; save() packt ein solches Stueck
	// deshalb auch nicht mit ein.
	static std::string resolveMusicPath(const std::string& musicFilename,
										const std::string& sourceDir);

	// Ein von aussen hereingereichtes Archiv annehmen. Getrennt in Pruefen und
	// Ablegen, damit der Aufrufer "das ist keine Kampagne" und "das Kopieren
	// ging schief" auseinanderhalten kann.
	static bool isImportableArchive(const std::string& archivePath);

	Campaign();
	~Campaign();

	void clear();

	// quiet unterdrueckt die Fehlermeldung. Das braucht nur
	// isImportableArchive(): etwas, das gar keine Kampagne sein will, ist auch
	// keine kaputte.
	bool load(const std::string& filename, bool quiet = false);
	bool loadInfo(TiXmlDocument* p_doc);
	bool save(const std::string& filename);
	TiXmlDocument* saveInfo();

	// Sind alle Levels lesbar? Liefert im Fehlerfall die fehlende Quelle.
	bool sourcesExist(std::string& missing) const;

	// Zustand fuer den Aenderungsvergleich des Editors: die XML-Daten plus
	// die Quellen, denn zwei Eintraege koennen denselben Namen tragen.
	std::string getStateString();

	void addLevel(const LevelRef& level);
	void removeLevelAt(int where);
	void swapLevels(int a, int b);
	bool hasLevel(const std::string& name) const;
	const std::vector<LevelRef>& getLevels() const;

	const std::string& getFilename() const;
	const std::string& getTitle() const;
	void setTitle(const std::string& title);
	const std::string& getDescription() const;
	void setDescription(const std::string& description);
	int getNumUnlockedLevels() const;
	void setNumUnlockedLevels(int numUnlockedLevels);
	bool hasBonusLevel() const;
	void setBonusLevel(bool haveOrNot);

private:
	std::string filename;
	std::string title;
	std::string description;
	std::vector<LevelRef> levels;
	int numUnlockedLevels;
	bool iHaveABonusLevel;
};

#endif