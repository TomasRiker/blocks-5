#ifndef _CAMPAIGN_H
#define _CAMPAIGN_H

/*** Class for a campaign ***/

class Campaign
{
public:
	// One level of a campaign. The identity is the pair (sourceDir, member);
	// "name" is nothing but display text out of campaign.xml and is never
	// composed into a path.
	struct LevelRef
	{
		LevelRef() : fromArchive(false) {}

		std::string source() const { return sourceDir + member; }

		std::string name;        // text from campaign.xml, display only
		std::string sourceDir;   // "<home>levels/" or "<campaign>.zip[pw]/"
		std::string member;      // filename or name of the archive member
		bool fromArchive;
	};

	// Reference to a loose file in the user's level folder.
	static LevelRef makeLooseRef(const std::string& filename);

	// The individual levels in the user's folder as a campaign that exists as
	// no file: that is how a level somebody sent you gets played without
	// opening it in the editor, which does not show the darkness and draws in
	// the teleporters' destinations. Returns false if there is nothing there.
	bool loadSingleLevels();

	// Is this that campaign? It carries no progress: everything is unlocked
	// from the start, nothing is recorded as finished, and after the last
	// diamond it goes back to the selection instead of to the next level.
	bool isSingleLevels() const;

	// Where a musicFilename points. sourceDir is the directory holding the
	// level's ordinary tracks - "<home>levels/" for a loose level,
	// "<campaign>.zip[pw]/" for one out of an archive. A name beginning with
	// "blocks:" means a track of the shipped campaign and sourceDir does not
	// matter; save() therefore does not pack such a track either.
	static std::string resolveMusicPath(const std::string& musicFilename,
										const std::string& sourceDir);

	// Accept an archive handed in from outside. Split into checking and
	// storing, which lets the caller tell "that is not a campaign" from "the
	// copy failed".
	static bool isImportableArchive(const std::string& archivePath);

	Campaign();
	~Campaign();

	void clear();

	// quiet suppresses the error message. Only isImportableArchive() needs
	// that: something that never claimed to be a campaign is not a broken one
	// either.
	bool load(const std::string& filename, bool quiet = false);
	bool loadInfo(TiXmlDocument* p_doc);
	bool save(const std::string& filename);
	TiXmlDocument* saveInfo();

	// Are all levels readable? Reports the missing source on failure.
	bool sourcesExist(std::string& missing) const;

	// State for the editor's change comparison: the XML data plus the
	// sources, because two entries can carry the same name.
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
	bool singleLevels;
};

#endif