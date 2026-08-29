#ifndef _CAMPAIGN_H
#define _CAMPAIGN_H

/*** Klasse für eine Kampagne ***/

class Campaign
{
public:
	// Ein Level einer Kampagne. Die Identitaet ist das Paar
	// (sourceDir, member); "name" ist blosser Anzeigetext - er steht in
	// campaign.xml und stammt bei einer fremden Kampagne aus einer fremden
	// Datei, wird also nie zu einem Pfad zusammengesetzt.
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

	Campaign();
	~Campaign();

	void clear();
	bool load(const std::string& filename);
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