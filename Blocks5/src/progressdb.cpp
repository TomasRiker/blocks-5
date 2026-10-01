#include "pch.h"
#include "progressdb.h"
#include "filesystem.h"

const std::string pw = "[3Cs18Ab0bV0Aat3Wf27le1ZM12kt0Xs05Aa4PX1EyI2V112Jr26v2GZO3dN0Ec91hk024P3cA32bc3GZ07Em4bf34st4320F7d13S00wd4Mg1ANn4SF2EO94Hz13Qq0LO18iY4Qy2C8r2XF28Bh]";

namespace
{
	// The one member of the archive.
	const char* const p_member = "/progress.xml";

	// A level index this large belongs to no campaign (one loads 500 levels at
	// most), so it can only come from a damaged or foreign file, and the
	// Manager imports foreign ones.
	const int MAX_LEVEL = 10000;
}

ProgressDB::ProgressDB()
{
}

ProgressDB::~ProgressDB()
{
}

std::string ProgressDB::getFilename()
{
	return FileSystem::inst().getAppHomeDirectory() + "progress.zip";
}

std::string ProgressDB::getBackupFilename()
{
	// Exists only while a save is in flight, so finding one means the last
	// save did not finish. Not ".bak", which retireShadowingCopies writes for
	// the opposite: a copy set aside for good.
	return getFilename() + ".saving";
}

bool ProgressDB::read(const std::string& filename,
					  Progress& progress)
{
	FileSystem& fs = FileSystem::inst();
	if(!fs.fileExists(filename)) return false;

	std::string xml = fs.readStringFromFile(filename + pw + p_member);
	TiXmlDocument doc;
	doc.SetCondenseWhiteSpace(false);
	doc.Parse(xml.c_str());

	// Nothing here is trusted, since the Manager imports this file: the member
	// may be missing (an empty string), the document may not parse, the root
	// may be something else, a campaign may carry no name and a level index
	// may be negative or absurd.
	TiXmlElement* p_progressDB = doc.FirstChildElement("ProgressDB");
	if(!p_progressDB) return false;

	TiXmlElement* p_campaignInfo = p_progressDB->FirstChildElement("CampaignInfo");
	while(p_campaignInfo)
	{
		const char* p_campaign = p_campaignInfo->Attribute("campaign");
		const std::string key(p_campaign ? keyFor(p_campaign) : std::string());

		if(!key.empty())
		{
			TiXmlElement* p_levelCompleted = p_campaignInfo->FirstChildElement("LevelCompleted");
			while(p_levelCompleted)
			{
				int level = -1;
				p_levelCompleted->QueryIntAttribute("level", &level);
				if(level >= 0 && level <= MAX_LEVEL) progress[key].insert(static_cast<uint>(level));

				p_levelCompleted = p_levelCompleted->NextSiblingElement("LevelCompleted");
			}
		}

		p_campaignInfo = p_campaignInfo->NextSiblingElement("CampaignInfo");
	}

	return true;
}

bool ProgressDB::write(const Progress& progress)
{
	TiXmlDocument doc;
	TiXmlDeclaration* p_decl = new TiXmlDeclaration("1.0", "", "");
	doc.LinkEndChild(p_decl);

	TiXmlElement* p_progressDB = new TiXmlElement("ProgressDB");

	for(Progress::const_iterator i = progress.begin(); i != progress.end(); ++i)
	{
		const std::string& campaign = i->first;
		const std::set<uint>& levelsCompleted = i->second;

		TiXmlElement* p_campaignInfo = new TiXmlElement("CampaignInfo");
		p_campaignInfo->SetAttribute("campaign", campaign);

		for(std::set<uint>::const_iterator j = levelsCompleted.begin(); j != levelsCompleted.end(); ++j)
		{
			TiXmlElement* p_levelCompleted = new TiXmlElement("LevelCompleted");
			p_levelCompleted->SetAttribute("level", *j);
			p_campaignInfo->LinkEndChild(p_levelCompleted);
		}

		p_progressDB->LinkEndChild(p_campaignInfo);
	}

	doc.LinkEndChild(p_progressDB);

	std::string xml;
	xml << doc;
	return FileSystem::inst().writeStringToFile(xml, getFilename() + pw + p_member);
}

ProgressDB::Progress ProgressDB::query(const std::string& filename,
									   bool* p_intact)
{
	FileSystem& fs = FileSystem::inst();
	const bool own = filename.empty();
	const std::string path(own ? getFilename() : filename);

	Progress progress;
	const bool readable = read(path, progress);
	bool intact = true;

	// Reading is where an interrupted save is put right, both ways: the backup
	// goes back where the database is missing or unreadable, and is deleted
	// where the database reads - left beside a good one it is from a run that
	// died long ago, and any later damage would restore a database months out
	// of date instead of reporting a fault.
	if(own)
	{
		const std::string backup(getBackupFilename());

		if(!readable && fs.fileExists(backup))
		{
			printfLog("+ The progress database is missing or unreadable - putting the backup back.\n");
			fs.deleteFile(path);
			if(fs.renameFile(backup, path))
			{
				progress.clear();

				// A backup that does not read either leaves nothing to
				// protect, so this stays intact: the next save may write over
				// it, the only way out of that state.
				read(path, progress);
			}
			else
			{
				// The player's whole progress still stands under the backup's
				// name; saying so stops the caller writing an empty database
				// over it.
				printfLog("+ ERROR: The backup could not be put back.\n");
				intact = false;
			}
		}
		else if(readable && fs.fileExists(backup))
		{
			fs.deleteFile(backup);
		}
	}

	if(p_intact) *p_intact = intact;
	return progress;
}

bool ProgressDB::markSolved(const std::vector<std::pair<std::string, uint> >& solved)
{
	if(solved.empty()) return true;

	FileSystem& fs = FileSystem::inst();

	// The disk first, so this adds to what is there now: two calls cannot lose
	// each other's entries, and a merge needs no code of its own.
	bool intact = false;
	Progress progress = query(std::string(), &intact);

	if(!intact)
	{
		// An interrupted save that could not be put back: what came back is
		// empty, and writing it would destroy the progress still standing
		// under the backup's name.
		printfLog("+ ERROR: The progress database is not in a state to be written to.\n");
		return false;
	}

	bool changed = false;
	for(size_t i = 0; i < solved.size(); i++)
	{
		if(progress[keyFor(solved[i].first)].insert(solved[i].second).second) changed = true;
	}

	if(!changed) return true;

	const std::string path(getFilename());
	const std::string backup(getBackupFilename());

	// The old database steps aside rather than being written over: putting a
	// member into a zip rebuilds the archive, and File_Archived deletes the old
	// file before the new one exists, so a crash or a full disk in that window
	// would leave the player with nothing.
	bool placedBackup = false;
	if(fs.fileExists(path))
	{
		if(!fs.renameFile(path, backup))
		{
			printfLog("+ ERROR: Could not set the progress database aside; it has not been written.\n");
			return false;
		}
		placedBackup = true;
	}

	if(write(progress))
	{
		// Only a backup this call put there; any other is an earlier
		// interrupted save's and not this call's to throw away.
		if(placedBackup) fs.deleteFile(backup);
		return true;
	}

	printfLog("+ ERROR: Could not write the progress database - putting the old one back.\n");
	if(placedBackup)
	{
		fs.deleteFile(path);
		fs.renameFile(backup, path);
	}
	return false;
}

bool ProgressDB::installFrom(const std::string& source)
{
	FileSystem& fs = FileSystem::inst();

	// An interrupted save is put right first, so that what gets replaced is
	// what the player would have had and no stale backup is left for a later
	// query to take for a fresh interruption.
	bool intact = false;
	query(std::string(), &intact);
	if(!intact) return false;

	const std::string path(getFilename());
	const std::string backup(getBackupFilename());

	// The same step aside as a save: copyFile opens the destination with "wb",
	// so a copy that fails halfway would leave neither database.
	bool placedBackup = false;
	if(fs.fileExists(path))
	{
		if(!fs.renameFile(path, backup)) return false;
		placedBackup = true;
	}

	if(fs.copyFile(source, path))
	{
		if(placedBackup) fs.deleteFile(backup);
		return true;
	}

	if(placedBackup)
	{
		fs.deleteFile(path);
		fs.renameFile(backup, path);
	}
	return false;
}

bool ProgressDB::exists()
{
	FileSystem& fs = FileSystem::inst();

	// Either name: right after an interrupted save the whole database stands
	// under the backup's, and "there is none" would let an import replace it
	// without asking.
	return fs.fileExists(getFilename()) || fs.fileExists(getBackupFilename());
}

bool ProgressDB::remove()
{
	FileSystem& fs = FileSystem::inst();

	// The backup goes too, or the next query would take it for an interrupted
	// save and put the deleted database back.
	fs.deleteFile(getBackupFilename());
	return fs.deleteFile(getFilename());
}

bool ProgressDB::isProgressArchive(const std::string& filename)
{
	// No password: a zip's table of contents lies open, so a file can be
	// recognised before anybody decides to trust it.
	return FileSystem::inst().fileExists(filename + p_member);
}

bool ProgressDB::canRead(const std::string& filename)
{
	Progress progress;
	return read(filename, progress);
}

std::string ProgressDB::keyFor(const std::string& campaign)
{
	// The bare filename, not the path, as Transfer::install() identifies a
	// campaign too. Progress then survives the file changing folder - the
	// shipped campaign lives in the game folder, an imported one in the user
	// directory - and a database written with full paths still reads right,
	// its keys stripped on the way in.
	//
	// Case is deliberately not folded: under Linux "Blocks.zip" and
	// "blocks.zip" are two campaigns, and joining their solved sets could not
	// be undone.
	return FileSystem::inst().getPathFilename(campaign);
}
