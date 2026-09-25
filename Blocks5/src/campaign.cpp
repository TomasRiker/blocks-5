#include "pch.h"
#include "campaign.h"
#include "engine.h"
#include "filesystem.h"
#include "progressdb.h"
#include "util.h"

const std::string pw = "[3Cs18Ab0bV0Aat3Wf27le1ZM12kt0Xs05Aa4PX1EyI2V112Jr26v2GZO3dN0Ec91hk024P3cA32bc3GZ07Em4bf34st4320F7d13S00wd4Mg1ANn4SF2EO94Hz13Qq0LO18iY4Qy2C8r2XF28Bh]";

// No campaign seriously holds more levels than this; the limit keeps a
// doctored campaign.xml from taking up a whole logic tick.
static const uint MAX_LEVELS = 500;

namespace
{
	// A campaign archive names its members after their position in the list,
	// never after the level: entry i is level_{i+1}.xml.
	std::string makeMemberName(uint index)
	{
		char temp[64] = "";
		sprintf(temp, "level_%d.xml", index + 1);
		return temp;
	}

	// A music track bound for the archive: under which name, out of which
	// source. The two can differ where a campaign mixes loose levels and
	// archive levels.
	struct MusicRef
	{
		std::string member;
		std::string source;
	};

	// The shipped campaign's archive, for "blocks:" music and for
	// isBuiltInCompleted().
	const char* const p_builtInPath = "levels/campaigns/blocks.zip";

	// The prefix with which a level names a music track of the shipped
	// campaign: musicFilename="blocks:music2.ogg".
	const char* const p_builtInMusicPrefix = "blocks:";

	bool isBuiltInMusic(const std::string& musicFilename)
	{
		const size_t n = strlen(p_builtInMusicPrefix);
		return musicFilename.length() > n &&
			   musicFilename.compare(0, n, p_builtInMusicPrefix) == 0;
	}
}

std::string Campaign::resolveMusicPath(const std::string& musicFilename,
									   const std::string& sourceDir)
{
	if(musicFilename.empty()) return "";
	if(!isBuiltInMusic(musicFilename)) return sourceDir + musicFilename;

	// What follows the colon is a member name, not a path: it stands in a
	// possibly foreign file and must open nothing but a track inside
	// blocks.zip.
	const std::string member(musicFilename.substr(strlen(p_builtInMusicPrefix)));
	if(!isSafeMemberName(member)) return "";

	return FileSystem::inst().resolveContentPath(p_builtInPath) + pw + "/" + member;
}

Campaign::LevelRef Campaign::makeLooseRef(const std::string& filename)
{
	LevelRef ref;
	ref.name = filename;
	// The folder hangs off the file: the two example levels sit with the
	// game, everything else with the player. sourceDir therefore carries the
	// path of the file found, minus its name, and not a fixed root.
	const std::string path(FileSystem::inst().resolveContentPath("levels/" + filename));
	ref.sourceDir = path.substr(0, path.length() - filename.length());
	ref.member = filename;
	ref.fromArchive = false;
	return ref;
}

bool Campaign::isImportableArchive(const std::string& archivePath)
{
	// 1. Structure: does the archive open, and does it hold a campaign.xml?
	//    Looking that up needs no password.
	if(!FileSystem::inst().fileExists(archivePath + "/campaign.xml")) return false;

	// 2. Content: decrypt, parse the XML, and at least one level.
	Campaign check;
	return check.load(archivePath, true) && !check.getLevels().empty();
}

// Finished means every level but the bonus one: getLevels().size() - 1 where
// there is a bonus level, the count that unlocks it in GS_Game::loadLevel and
// GS_SelectLevel::getLevelStatus, since the bonus is extra rather than the
// end of the run. A player who reached the credits by playing is past it
// either way: GS_Game records the last level before it hands over. Asked of
// the database rather than a flag, so an imported progress counts exactly as
// playing would. No archive (an unpacked tree), one that will not parse, an
// empty campaign and no progress all answer false.
bool Campaign::isBuiltInCompleted()
{
	FileSystem& fs = FileSystem::inst();
	const std::string path(fs.resolveContentPath(p_builtInPath));
	if(!fs.fileExists(path + "/campaign.xml")) return false;

	// Quiet: a campaign that will not load says so with a toast where the
	// player asked for it, and this is a question nobody asked out loud.
	Campaign campaign;
	if(!campaign.load(path, true)) return false;

	const size_t levels = campaign.getLevels().size();
	const size_t needed = (campaign.hasBonusLevel() && levels) ? levels - 1 : levels;
	if(!needed) return false;

	const ProgressDB::Progress progress = ProgressDB::inst().query();
	const ProgressDB::Progress::const_iterator entry =
		progress.find(ProgressDB::keyFor(campaign.getFilename()));
	const size_t completed = (entry == progress.end()) ? 0 : entry->second.size();

	return completed >= needed;
}

Campaign::Campaign()
{
	// clear() sets all three too, but only once somebody calls it; until then
	// getNumUnlockedLevels() would answer garbage.
	numUnlockedLevels = 1;
	iHaveABonusLevel = false;
	singleLevels = false;
}

Campaign::~Campaign()
{
	clear();
}

void Campaign::clear()
{
	filename = "";
	title = loadString("$CE_DEFAULT_CAMPAIGN_TITLE");
	description = loadString("$CE_DEFAULT_CAMPAIGN_DESCRIPTION");
	levels.clear();
	numUnlockedLevels = 1;
	iHaveABonusLevel = false;
	singleLevels = false;
}

// Just the title out of a level file, without building the level: a parse of
// the whole document (4 to 25 KB), still far cheaper than a Level::load with
// its objects and skins. Returned localized, because the sorting runs on it
// and the default title carries both languages in one string.
static std::string readLevelTitle(const std::string& source)
{
	TiXmlDocument doc;
	doc.SetCondenseWhiteSpace(false);
	doc.Parse(FileSystem::inst().readStringFromFile(source).c_str());

	TiXmlElement* p_level = doc.FirstChildElement("Level");
	const char* p_title = p_level ? p_level->Attribute("title") : 0;
	if(!p_title || !*p_title) return localizeString("\xA7" "en:Unnamed Level\xA7" "de:Unbenannter Level");
	return localizeString(p_title);
}

// By the title as shown, not by the filename. Case-insensitive, or every
// capitalized title would sort before the lower-case ones; folded by hand,
// since tolower depends on the locale. Equal titles fall back to the
// filename, which makes the order total.
static bool byTitle(const Campaign::LevelRef& a,
					const Campaign::LevelRef& b)
{
	const char* p_a = a.name.c_str();
	const char* p_b = b.name.c_str();
	while(*p_a && *p_b)
	{
		const unsigned char ca = static_cast<unsigned char>(*p_a >= 'A' && *p_a <= 'Z' ? *p_a + 32 : *p_a);
		const unsigned char cb = static_cast<unsigned char>(*p_b >= 'A' && *p_b <= 'Z' ? *p_b + 32 : *p_b);
		if(ca != cb) return ca < cb;
		++p_a;
		++p_b;
	}

	if(*p_a != *p_b) return *p_b != 0;
	return a.member < b.member;
}

bool Campaign::loadSingleLevels()
{
	clear();

	FileSystem& fs = FileSystem::inst();
	// Both roots: the game brings the example levels, the rest the player
	// built or imported.
	std::list<std::string> files = fs.listDirectory(fs.getGameDirectory() + "levels");
	const std::list<std::string> own(fs.listDirectory(fs.getAppHomeDirectory() + "levels"));
	files.insert(files.end(), own.begin(), own.end());
	files.sort();
	files.unique();

	for(std::list<std::string>::const_iterator i = files.begin(); i != files.end(); ++i)
	{
		if(i->length() <= 4 || i->substr(i->length() - 4) != ".xml") continue;

		LevelRef ref = makeLooseRef(*i);
		ref.name = readLevelTitle(ref.source());
		addLevel(ref);
	}

	if(levels.empty()) return false;

	std::sort(levels.begin(), levels.end(), byTitle);

	singleLevels = true;
	title = loadString("$LS_SINGLE_LEVELS");
	description = loadString("$LS_SINGLE_LEVELS_DESCRIPTION");

	// Everything unlocked: levels that have nothing to do with each other have
	// no order to earn.
	numUnlockedLevels = static_cast<int>(levels.size());
	return true;
}

bool Campaign::isSingleLevels() const
{
	return singleLevels;
}

bool Campaign::load(const std::string& filename,
					bool quiet)
{
	clear();
	this->filename = filename;

	// load the XML document
	std::string text = FileSystem::inst().readStringFromFile(filename + pw + "/campaign.xml");
	TiXmlDocument doc;
	doc.SetCondenseWhiteSpace(false);
	doc.Parse(text.c_str());
	if(doc.ErrorId())
	{
		printfLog("+ ERROR: Could not parse campaign XML file \"%s\" (Error: %d).\n",
				  (filename + "/campaign.xml").c_str(),
				  doc.ErrorId());
	}
	else if(loadInfo(&doc)) return true;

	// All three failure paths meet here: broken XML, no <Campaign>, too many
	// levels. The toast names the archive's filename, not its whole path.
	if(!quiet)
	{
		const std::string::size_type slash = filename.find_last_of('/');
		Engine::inst().showToast(Engine::TOAST_ERROR,
								 localizeString("$ERROR_CAMPAIGN_INVALID") + " \"" +
								 (slash == std::string::npos ? filename : filename.substr(slash + 1)) + "\"");
	}

	return false;
}

bool Campaign::loadInfo(TiXmlDocument* p_doc)
{
	TiXmlElement* p_campaign = p_doc->FirstChildElement("Campaign");
	if(!p_campaign)
	{
		printfLog("+ ERROR: Campaign XML file \"%s\" is invalid.\n",
				  (filename + "/campaign.xml").c_str());
		return false;
	}

	// read the title
	TiXmlElement* p_title = p_campaign->FirstChildElement("Title");
	if(p_title)
	{
		const char* p_text = p_title->GetText();
		if(p_text) title = p_text;
	}

	// read the description
	TiXmlElement* p_description = p_campaign->FirstChildElement("Description");
	if(p_description)
	{
		const char* p_text = p_description->GetText();
		if(p_text) description = p_text;
	}

	// read the levels
	TiXmlElement* p_levels = p_campaign->FirstChildElement("Levels");
	if(p_levels)
	{
		// 1. First pass: collect just the names.
		std::vector<std::string> names;
		TiXmlElement* p_level = p_levels->FirstChildElement("Level");
		while(p_level)
		{
			if(names.size() >= MAX_LEVELS)
			{
				printfLog("+ ERROR: Campaign \"%s\" lists more than %u levels.\n",
						  filename.c_str(), MAX_LEVELS);
				return false;
			}

			const char* p_text = p_level->GetText();
			if(p_text) names.push_back(p_text);
			p_level = p_level->NextSiblingElement("Level");
		}

		// 2. Where do the levels come from? If ALL of them lie loose in the
		//    level folder, the campaign was made here and is served from the
		//    loose files; otherwise ALL come out of the archive, entry i as
		//    member level_{i+1}.xml. All or nothing, or a foreign campaign
		//    would quietly pick up a level of the user's with the same name.
		FileSystem& fs = FileSystem::inst();

		bool allLoose = !names.empty();
		for(uint i = 0; i < names.size() && allLoose; i++)
		{
			// Asked over both roots, exactly as makeLooseRef() resolves them -
			// otherwise an example level sitting with the game would count as
			// missing.
			if(!isSafeMemberName(names[i]) ||
			   !fs.fileExists(fs.resolveContentPath("levels/" + names[i]))) allLoose = false;
		}

		for(uint i = 0; i < names.size(); i++)
		{
			if(allLoose) addLevel(makeLooseRef(names[i]));
			else
			{
				LevelRef ref;
				ref.name = names[i];                       // display text only
				ref.sourceDir = filename + pw + "/";
				ref.member = makeMemberName(i);            // from the index, never from the text
				ref.fromArchive = true;
				addLevel(ref);
			}
		}

		p_levels->QueryIntAttribute("numUnlockedLevels", &numUnlockedLevels);

		int bonusLevel = 0;
		p_levels->QueryIntAttribute("bonusLevel", &bonusLevel);
		iHaveABonusLevel = bonusLevel ? true : false;
	}

	return true;
}

bool Campaign::save(const std::string& filename)
{
	if(levels.empty()) return false;

	FileSystem& fs = FileSystem::inst();

	// Write to a side file first, then swap: a campaign whose levels come out
	// of its own archive would otherwise destroy exactly the levels that are
	// still to be read.
	const std::string temp(fs.getAppHomeDirectory() + "~campaignsave.zip");

	// A leftover from an aborted save has to go: File_Archived opens an
	// existing archive in append mode and would drag its old members along.
	if(fs.fileExists(temp)) fs.deleteFile(temp);

	// Are all the sources readable at all? Nothing has been written up to here.
	std::string missing;
	if(!sourcesExist(missing))
	{
		printfLog("+ ERROR: Cannot save campaign, level source \"%s\" is missing.\n", missing.c_str());
		return false;
	}

	// write the XML data
	TiXmlDocument* p_doc = saveInfo();
	std::string xml;
	xml << *p_doc;
	delete p_doc;
	if(!fs.writeStringToFile(xml, temp + pw + "/campaign.xml"))
	{
		fs.deleteFile(temp);
		return false;
	}

	// insert the levels, one after another
	std::vector<MusicRef> music;
	for(uint i = 0; i < levels.size(); i++)
	{
		// Is this a level at all? The root node is enough and brings the music
		// name along. readStringFromFile stops at a null byte, harmless for
		// XML, and the bytes themselves travel by copyFile below.
		const std::string levelXML(fs.readStringFromFile(levels[i].source()));
		TiXmlDocument doc;
		doc.SetCondenseWhiteSpace(false);
		doc.Parse(levelXML.c_str());
		TiXmlElement* p_levelNode = doc.ErrorId() ? 0 : doc.FirstChildElement("Level");
		if(!p_levelNode)
		{
			printfLog("+ ERROR: \"%s\" is not a valid level file.\n", levels[i].source().c_str());
			fs.deleteFile(temp);
			return false;
		}

		// insert the level into the archive - byte for byte, keeping the <Row>
		// lines with their raw tile codes from ever being written out afresh.
		if(!fs.copyFile(levels[i].source(), temp + pw + "/" + makeMemberName(i)))
		{
			fs.deleteFile(temp);
			return false;
		}

		// Remember the music filename. It comes from a possibly foreign file
		// and is checked before it joins a path, or the archive would pack
		// whatever an attacker names.
		const char* p_music = p_levelNode->Attribute("musicFilename");
		if(!p_music || !*p_music) continue;

		const std::string track(p_music);

		// A "blocks:" track lies in the shipped campaign, which everybody has.
		// Packing it in here would grow the archive by megabytes, and playback
		// reads blocks.zip anyway.
		if(isBuiltInMusic(track)) continue;

		if(!isSafeMemberName(track) || track == "campaign.xml")
		{
			printfLog("+ WARNING: Level \"%s\" names an unusable music file - skipped.\n",
					  levels[i].source().c_str());
			continue;
		}

		MusicRef entry;
		entry.member = track;
		entry.source = levels[i].sourceDir + track;

		bool known = false;
		for(uint j = 0; j < music.size(); j++)
		{
			if(music[j].member != entry.member) continue;
			known = true;
			if(music[j].source != entry.source)
			{
				printfLog("+ WARNING: Two music files are called \"%s\" (\"%s\" and \"%s\") - the first one wins.\n",
						  entry.member.c_str(), music[j].source.c_str(), entry.source.c_str());
			}
		}
		if(!known) music.push_back(entry);
	}

	// insert the music tracks. A missing file is not an error.
	for(uint i = 0; i < music.size(); i++)
	{
		if(!fs.fileExists(music[i].source))
		{
			printfLog("+ WARNING: Music file \"%s\" is missing - not stored.\n", music[i].source.c_str());
			continue;
		}

		if(!fs.copyFile(music[i].source, temp + pw + "/" + music[i].member))
		{
			fs.deleteFile(temp);
			return false;
		}
	}

	// Swap, by a rename where the platform can: the archive (megabytes with
	// its music) is not written a second time, and the old file is replaced in
	// one step rather than truncated and refilled.
	if(!fs.renameFile(temp, filename))
	{
		fs.deleteFile(temp);
		return false;
	}

	// Point the archive-backed references at the new archive, which is what
	// makes a second save right. Loose references stay loose - otherwise a
	// level edited afterwards would no longer come through.
	this->filename = filename;
	for(uint i = 0; i < levels.size(); i++)
	{
		if(!levels[i].fromArchive) continue;
		levels[i].sourceDir = filename + pw + "/";
		levels[i].member = makeMemberName(i);
	}

	return true;
}

TiXmlDocument* Campaign::saveInfo()
{
	TiXmlDocument* p_doc = new TiXmlDocument;

	TiXmlDeclaration* p_decl = new TiXmlDeclaration("1.0", "", "");
	p_doc->LinkEndChild(p_decl);

	TiXmlElement* p_campaign = new TiXmlElement("Campaign");

	// write the title
	TiXmlElement* p_title = new TiXmlElement("Title");
	p_title->LinkEndChild(new TiXmlText(title));
	p_campaign->LinkEndChild(p_title);

	// write the description
	TiXmlElement* p_description = new TiXmlElement("Description");
	p_description->LinkEndChild(new TiXmlText(description));
	p_campaign->LinkEndChild(p_description);

	// write the list of levels
	TiXmlElement* p_levels = new TiXmlElement("Levels");
	for(uint i = 0; i < levels.size(); i++)
	{
		TiXmlElement* p_level = new TiXmlElement("Level");
		p_level->LinkEndChild(new TiXmlText(levels[i].name));
		p_levels->LinkEndChild(p_level);
	}
	p_levels->SetAttribute("numUnlockedLevels", numUnlockedLevels);
	p_levels->SetAttribute("bonusLevel", iHaveABonusLevel ? 1 : 0);
	p_campaign->LinkEndChild(p_levels);

	p_doc->LinkEndChild(p_campaign);

	return p_doc;
}

bool Campaign::sourcesExist(std::string& missing) const
{
	for(uint i = 0; i < levels.size(); i++)
	{
		if(!FileSystem::inst().fileExists(levels[i].source()))
		{
			missing = levels[i].source();
			return false;
		}
	}

	return true;
}

std::string Campaign::getStateString()
{
	TiXmlDocument* p_doc = saveInfo();
	std::string state;
	state << *p_doc;
	delete p_doc;

	// The XML data alone is not enough: two entries may carry the same name,
	// and a reordering would otherwise be no change at all.
	for(uint i = 0; i < levels.size(); i++)
	{
		state += "\n";
		state += levels[i].fromArchive ? "A|" : "L|";
		state += levels[i].source();
	}

	return state;
}

void Campaign::addLevel(const LevelRef& level)
{
	levels.push_back(level);
}

void Campaign::removeLevelAt(int where)
{
	if(where < 0 || where >= static_cast<int>(levels.size())) return;
	levels.erase(levels.begin() + where);
}

void Campaign::swapLevels(int a, int b)
{
	const int n = static_cast<int>(levels.size());
	if(a < 0 || b < 0 || a >= n || b >= n || a == b) return;

	const LevelRef temp(levels[a]);
	levels[a] = levels[b];
	levels[b] = temp;
}

bool Campaign::hasLevel(const std::string& name) const
{
	for(uint i = 0; i < levels.size(); i++)
	{
		if(levels[i].name == name) return true;
	}

	return false;
}

const std::vector<Campaign::LevelRef>& Campaign::getLevels() const
{
	return levels;
}

const std::string& Campaign::getFilename() const
{
	return filename;
}

const std::string& Campaign::getTitle() const
{
	return title;
}

void Campaign::setTitle(const std::string& title)
{
	this->title = title;
}

const std::string& Campaign::getDescription() const
{
	return description;
}

void Campaign::setDescription(const std::string& description)
{
	this->description = description;
}

int Campaign::getNumUnlockedLevels() const
{
	return numUnlockedLevels;
}

void Campaign::setNumUnlockedLevels(int numUnlockedLevels)
{
	this->numUnlockedLevels = numUnlockedLevels;
}

bool Campaign::hasBonusLevel() const
{
	return iHaveABonusLevel;
}

void Campaign::setBonusLevel(bool haveOrNot)
{
	iHaveABonusLevel = haveOrNot;
}