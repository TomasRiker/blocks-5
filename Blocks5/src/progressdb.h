#ifndef _PROGRESSDB_H
#define _PROGRESSDB_H

#include "singleton.h"

/*** Klasse zum Speichern und Abrufen des Fortschritts ***/

class ProgressDB : public Singleton<ProgressDB>
{
	friend class Singleton<ProgressDB>;

public:
	void load();
	void save();

	uint getNumLevelsCompleted(const std::string& campaign);
	bool wasLevelCompleted(const std::string& campaign, uint level);

	void setLevelCompleted(const std::string& campaign, uint level);

private:
	ProgressDB();
	~ProgressDB();

	struct CampaignInfo
	{
		std::set<uint> levelsCompleted;
	};

	typedef stdext::hash_map<std::string, CampaignInfo> dbMap;

	dbMap db;
};

#endif