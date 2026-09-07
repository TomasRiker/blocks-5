#ifndef _PROGRESSDB_H
#define _PROGRESSDB_H

#include "singleton.h"

/*** Class for saving and retrieving progress ***/

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
	// The key to a campaign: its bare filename. See progressdb.cpp - the full
	// path will not do, because a campaign file can change folder.
	static std::string keyFor(const std::string& campaign);

	ProgressDB();
	~ProgressDB();

	struct CampaignInfo
	{
		std::set<uint> levelsCompleted;
	};

	typedef std::unordered_map<std::string, CampaignInfo> dbMap;

	dbMap db;
};

#endif