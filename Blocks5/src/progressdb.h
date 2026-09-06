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
	// Der Schluessel zu einer Kampagne: ihr blosser Dateiname. Siehe
	// progressdb.cpp - der volle Pfad taugte nicht, weil eine Kampagnendatei
	// den Ordner wechseln kann.
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