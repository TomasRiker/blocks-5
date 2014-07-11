#include "pch.h"
#include "progressdb.h"
#include "filesystem.h"

const std::string pw = "[3Cs18Ab0bV0Aat3Wf27le1ZM12kt0Xs05Aa4PX1EyI2V112Jr26v2GZO3dN0Ec91hk024P3cA32bc3GZ07Em4bf34st4320F7d13S00wd4Mg1ANn4SF2EO94Hz13Qq0LO18iY4Qy2C8r2XF28Bh]";

ProgressDB::ProgressDB()
{
}

ProgressDB::~ProgressDB()
{
}

void ProgressDB::load()
{
	FileSystem& fs = FileSystem::inst();
	if(!fs.fileExists(FileSystem::inst().getAppHomeDirectory() + "progress.zip")) return;

	// XML-Dokument laden
	std::string xml = fs.readStringFromFile(FileSystem::inst().getAppHomeDirectory() + "progress.zip" + pw + "/progress.xml");
	TiXmlDocument doc;
	doc.SetCondenseWhiteSpace(false);
	doc.Parse(xml.c_str());

	TiXmlElement* p_progressDB = doc.FirstChildElement("ProgressDB");
	TiXmlElement* p_campaignInfo = p_progressDB->FirstChildElement("CampaignInfo");
	while(p_campaignInfo)
	{
		std::string campaign = p_campaignInfo->Attribute("campaign");
		TiXmlElement* p_levelCompleted = p_campaignInfo->FirstChildElement("LevelCompleted");
		while(p_levelCompleted)
		{
			int level = -1;
			p_levelCompleted->QueryIntAttribute("level", &level);
			if(level != -1) setLevelCompleted(campaign, level);

			p_levelCompleted = p_levelCompleted->NextSiblingElement("LevelCompleted");
		}

		p_campaignInfo = p_campaignInfo->NextSiblingElement("CampaignInfo");
	}
}

void ProgressDB::save()
{
	TiXmlDocument doc;
	TiXmlDeclaration* p_decl = new TiXmlDeclaration("1.0", "", "");
	doc.LinkEndChild(p_decl);

	TiXmlElement* p_progressDB = new TiXmlElement("ProgressDB");

	for(dbMap::const_iterator i = db.begin(); i != db.end(); ++i)
	{
		const std::string& campaign = i->first;
		const std::set<uint>& levelsCompleted = i->second.levelsCompleted;

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
	FileSystem::inst().writeStringToFile(xml, FileSystem::inst().getAppHomeDirectory() + "progress.zip" + pw + "/progress.xml");
}

uint ProgressDB::getNumLevelsCompleted(const std::string& campaign)
{
	dbMap::const_iterator i = db.find(campaign);
	if(i != db.end()) return static_cast<uint>(i->second.levelsCompleted.size());
	else return 0;
}

bool ProgressDB::wasLevelCompleted(const std::string& campaign,
								   uint level)
{
	dbMap::const_iterator i = db.find(campaign);
	if(i != db.end())
	{
		const std::set<uint>& levelsCompleted = i->second.levelsCompleted;
		return levelsCompleted.find(level) != levelsCompleted.end();
	}
	else return false;
}

void ProgressDB::setLevelCompleted(const std::string& campaign,
								   uint level)
{
	db[campaign].levelsCompleted.insert(level);
}