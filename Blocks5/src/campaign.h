#ifndef _CAMPAIGN_H
#define _CAMPAIGN_H

/*** Klasse für eine Kampagne ***/

class Campaign
{
public:
	Campaign();
	~Campaign();

	void clear();
	bool load(const std::string& filename);
	bool loadInfo(TiXmlDocument* p_doc);
	bool save(const std::string& filename);
	TiXmlDocument* saveInfo();

	bool originalLevelsExist();
	void addLevel(const std::string& level);
	void insertLevel(int where, const std::string& level);
	void removeLevel(const std::string& level);
	bool hasLevel(const std::string& level);
	const std::vector<std::string>& getLevels() const;

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
	std::vector<std::string> levels;
	int numUnlockedLevels;
	bool iHaveABonusLevel;
};

#endif