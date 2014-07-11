#include "pch.h"
#include "campaign.h"
#include "filesystem.h"
#include "level.h"
#include "tileset.h"
#include "font.h"
#include "texture.h"

const std::string pw = "[3Cs18Ab0bV0Aat3Wf27le1ZM12kt0Xs05Aa4PX1EyI2V112Jr26v2GZO3dN0Ec91hk024P3cA32bc3GZ07Em4bf34st4320F7d13S00wd4Mg1ANn4SF2EO94Hz13Qq0LO18iY4Qy2C8r2XF28Bh]";

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
		TiXmlElement* p_level = p_levels->FirstChildElement("Level");
		while(p_level)
		{
			const char* p_text = p_level->GetText();
			if(p_text) addLevel(p_text);
			p_level = p_level->NextSiblingElement("Level");
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
	bool result = true;

	// Wenn die Archivdatei schon existiert, wird sie gelöscht.
	if(fs.fileExists(filename)) fs.deleteFile(filename);

	// XML-Daten schreiben
	TiXmlDocument* p_doc = saveInfo();
	std::string xml;
	xml << *p_doc;
	bool r = fs.writeStringToFile(xml, filename + pw + "/campaign.xml");
	delete p_doc;
	if(!r) result = false;

	std::set<std::string> filesToAdd;

	// Levels und Musikstücke einfügen
	Level* p_level = 0;
	for(uint i = 0; i < levels.size(); i++)
	{
		// Level laden
		Level* p_oldLevel = p_level;
		p_level = new Level;
		p_level->setInEditor(true);
		if(p_level->load(FileSystem::inst().getAppHomeDirectory() + "levels/" + levels[i], true))
		{
			if(!p_level->getMusicFilename().empty()) filesToAdd.insert(FileSystem::inst().getAppHomeDirectory() + "levels/" + p_level->getMusicFilename());

			// Level ins Archiv einfügen
			char newFilename[256] = "";
			sprintf(newFilename, "level_%d.xml", i + 1);
			if(!fs.copyFile(FileSystem::inst().getAppHomeDirectory() + "levels/" + levels[i],
							filename + pw + "/" + newFilename)) result = false;
		}
		else result = false;

		delete p_oldLevel;
	}

	// die Dateien ins Archiv packen
	for(std::set<std::string>::const_iterator i = filesToAdd.begin(); i != filesToAdd.end(); ++i)
	{
		if(fs.fileExists(*i))
		{
			if(!fs.copyFile(*i,
							filename + pw + "/" + fs.getPathFilename(*i))) result = false;
		}
	}

	return result;
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
		p_level->LinkEndChild(new TiXmlText(levels[i]));
		p_levels->LinkEndChild(p_level);
	}
	p_levels->SetAttribute("numUnlockedLevels", numUnlockedLevels);
	p_levels->SetAttribute("bonusLevel", iHaveABonusLevel ? 1 : 0);
	p_campaign->LinkEndChild(p_levels);

	p_doc->LinkEndChild(p_campaign);

	return p_doc;
}

bool Campaign::originalLevelsExist()
{
	for(uint i = 0; i < levels.size(); i++)
	{
		if(!FileSystem::inst().fileExists(FileSystem::inst().getAppHomeDirectory() + "levels/" + levels[i])) return false;
	}

	return true;
}

void Campaign::addLevel(const std::string& level)
{
	levels.push_back(level);
}

void Campaign::insertLevel(int where,
						   const std::string& level)
{
	levels.insert(levels.begin() + where, level);
}

void Campaign::removeLevel(const std::string& level)
{
	for(uint i = 0; i < levels.size(); i++)
	{
		if(levels[i] == level)
		{
			levels.erase(levels.begin() + i);
			return;
		}
	}
}

bool Campaign::hasLevel(const std::string& level)
{
	for(uint i = 0; i < levels.size(); i++)
	{
		if(levels[i] == level) return true;
	}

	return false;
}

const std::vector<std::string>& Campaign::getLevels() const
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