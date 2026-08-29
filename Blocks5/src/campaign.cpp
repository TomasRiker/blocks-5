#include "pch.h"
#include "campaign.h"
#include "filesystem.h"
#include "util.h"

const std::string pw = "[3Cs18Ab0bV0Aat3Wf27le1ZM12kt0Xs05Aa4PX1EyI2V112Jr26v2GZO3dN0Ec91hk024P3cA32bc3GZ07Em4bf34st4320F7d13S00wd4Mg1ANn4SF2EO94Hz13Qq0LO18iY4Qy2C8r2XF28Bh]";

// Mehr Levels als das nimmt keine Kampagne ernsthaft an; die Schranke haelt
// eine praeparierte campaign.xml davon ab, einen Logiktakt zu belegen.
static const uint MAX_LEVELS = 500;

namespace
{
	// Die Mitglieder eines Kampagnenarchivs heissen seit jeher nach ihrer
	// Position in der Liste, nicht nach dem Level (campaign.cpp, save()).
	std::string makeMemberName(uint index)
	{
		char temp[64] = "";
		sprintf(temp, "level_%d.xml", index + 1);
		return temp;
	}

	// Ein Musikstueck, das ins Archiv soll: unter welchem Namen, aus welcher
	// Quelle. Beides kann sich unterscheiden, wenn eine Kampagne lose Levels
	// und Archiv-Levels mischt.
	struct MusicRef
	{
		std::string member;
		std::string source;
	};
}

Campaign::LevelRef Campaign::makeLooseRef(const std::string& filename)
{
	LevelRef ref;
	ref.name = filename;
	ref.sourceDir = FileSystem::inst().getAppHomeDirectory() + "levels/";
	ref.member = filename;
	ref.fromArchive = false;
	return ref;
}

Campaign::Campaign()
{
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
}

bool Campaign::load(const std::string& filename)
{
	clear();
	this->filename = filename;

	// XML-Dokument laden
	std::string text = FileSystem::inst().readStringFromFile(filename + pw + "/campaign.xml");
	TiXmlDocument doc;
	doc.SetCondenseWhiteSpace(false);
	doc.Parse(text.c_str());
	if(doc.ErrorId())
	{
		printfLog("+ ERROR: Could not parse campaign XML file \"%s\" (Error: %d).\n",
				  (filename + "/campaign.xml").c_str(),
				  doc.ErrorId());
		return false;
	}

	return loadInfo(&doc);
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

	// Titel lesen
	TiXmlElement* p_title = p_campaign->FirstChildElement("Title");
	if(p_title)
	{
		const char* p_text = p_title->GetText();
		if(p_text) title = p_text;
	}

	// Beschreibung lesen
	TiXmlElement* p_description = p_campaign->FirstChildElement("Description");
	if(p_description)
	{
		const char* p_text = p_description->GetText();
		if(p_text) description = p_text;
	}

	// Levels lesen
	TiXmlElement* p_levels = p_campaign->FirstChildElement("Levels");
	if(p_levels)
	{
		// 1. Durchgang: nur die Namen einsammeln.
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

		// 2. Woher kommen die Levels? Liegen ALLE Originale lose im
		//    Level-Ordner, ist die Kampagne hier entstanden und wird weiter
		//    aus den losen Dateien bedient - unveraendertes Verhalten. Sonst
		//    kommt sie von woanders (oder ist wie die mitgelieferte
		//    blocks.zip auseinandergelaufen), und dann werden ALLE Levels
		//    aus dem Archiv gelesen: Eintrag i ist Mitglied level_{i+1}.xml,
		//    genau so, wie save() sie schreibt und das Spiel sie abspielt.
		//    Alles oder nichts, damit eine fremde Kampagne nie stillschweigend
		//    einen gleichnamigen Level des Benutzers einsammelt.
		FileSystem& fs = FileSystem::inst();
		const std::string looseDir(fs.getAppHomeDirectory() + "levels/");

		bool allLoose = !names.empty();
		for(uint i = 0; i < names.size() && allLoose; i++)
		{
			if(!isSafeMemberName(names[i]) || !fs.fileExists(looseDir + names[i])) allLoose = false;
		}

		for(uint i = 0; i < names.size(); i++)
		{
			if(allLoose) addLevel(makeLooseRef(names[i]));
			else
			{
				LevelRef ref;
				ref.name = names[i];                       // nur Anzeigetext
				ref.sourceDir = filename + pw + "/";
				ref.member = makeMemberName(i);            // aus dem Index, nie aus dem Text
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

	// Erst in eine Nebendatei schreiben, dann tauschen. Anders geht es nicht:
	// eine Kampagne, deren Levels aus ihrem eigenen Archiv kommen, wuerde
	// beim alten Vorgehen - Ziel zuerst loeschen - genau die Levels
	// vernichten, die noch gelesen werden sollen.
	const std::string temp(fs.getAppHomeDirectory() + "~campaignsave.zip");

	// Ein Rest von einem abgebrochenen Speichern muss weg: File_Archived
	// oeffnet ein vorhandenes Archiv im Anhaengemodus und wuerde dessen alte
	// Mitglieder mitschleppen.
	if(fs.fileExists(temp)) fs.deleteFile(temp);

	// Sind alle Quellen ueberhaupt lesbar? Bis hierher wurde nichts geschrieben.
	std::string missing;
	if(!sourcesExist(missing))
	{
		printfLog("+ ERROR: Cannot save campaign, level source \"%s\" is missing.\n", missing.c_str());
		return false;
	}

	// XML-Daten schreiben
	TiXmlDocument* p_doc = saveInfo();
	std::string xml;
	xml << *p_doc;
	delete p_doc;
	if(!fs.writeStringToFile(xml, temp + pw + "/campaign.xml"))
	{
		fs.deleteFile(temp);
		return false;
	}

	// Levels einfuegen, einen nach dem anderen
	std::vector<MusicRef> music;
	for(uint i = 0; i < levels.size(); i++)
	{
		// Ist das ueberhaupt ein Level? Frueher hat Level::load das geprueft;
		// hier reicht der Wurzelknoten, und der Musikname wird gleich
		// mitgelesen. readStringFromFile bricht am ersten Nullbyte ab - fuer
		// XML unerheblich, und die Bytes selbst wandern unten per copyFile.
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

		// Level ins Archiv einfuegen - Byte fuer Byte, damit die <Row>-Zeilen
		// mit den rohen Kachelcodes nie neu serialisiert werden.
		if(!fs.copyFile(levels[i].source(), temp + pw + "/" + makeMemberName(i)))
		{
			fs.deleteFile(temp);
			return false;
		}

		// Musikdateinamen vormerken. Der Name steht in einer moeglicherweise
		// fremden Datei und darf deshalb nicht ungeprueft an einen Pfad
		// gehaengt werden - sonst packt das Archiv, was der Angreifer nennt.
		const char* p_music = p_levelNode->Attribute("musicFilename");
		if(!p_music || !*p_music) continue;

		const std::string track(p_music);
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

	// Musikstuecke einfuegen. Eine fehlende Datei ist kein Fehler - das war
	// auch bisher so.
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

	// Tauschen. copyFile oeffnet das Ziel mit "wb", schneidet es also ab.
	if(!fs.copyFile(temp, filename))
	{
		fs.deleteFile(temp);
		return false;
	}
	fs.deleteFile(temp);

	// Archivgestuetzte Verweise auf das neue Archiv umbiegen, damit ein
	// zweites Speichern stimmt. Lose Verweise bleiben lose - sonst wuerde
	// ein danach im Level-Editor bearbeiteter Level nicht mehr durchschlagen.
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

	// Titel schreiben
	TiXmlElement* p_title = new TiXmlElement("Title");
	p_title->LinkEndChild(new TiXmlText(title));
	p_campaign->LinkEndChild(p_title);

	// Beschreibung schreiben
	TiXmlElement* p_description = new TiXmlElement("Description");
	p_description->LinkEndChild(new TiXmlText(description));
	p_campaign->LinkEndChild(p_description);

	// Liste der Levels schreiben
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

	// Die XML-Daten allein reichen nicht: zwei Eintraege duerfen denselben
	// Namen tragen, und ein Umsortieren waere sonst keine Aenderung.
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