#include "pch.h"
#include "gs_selectlevel.h"
#include "gui.h"
#include "gui_all.h"
#include "cf_all.h"
#include "texture.h"
#include "filesystem.h"
#include "campaign.h"
#include "level.h"
#include "progressdb.h"

const std::string pw = "[3Cs18Ab0bV0Aat3Wf27le1ZM12kt0Xs05Aa4PX1EyI2V112Jr26v2GZO3dN0Ec91hk024P3cA32bc3GZ07Em4bf34st4320F7d13S00wd4Mg1ANn4SF2EO94Hz13Qq0LO18iY4Qy2C8r2XF28Bh]";

GS_SelectLevel::GS_SelectLevel() : GameState("GS_SelectLevel"), engine(Engine::inst())
{
	p_background = 0;
	p_misc = 0;
	p_currentCampaign = 0;
	currentLevel = 0;
	p_currentLevel = 0;
}

GS_SelectLevel::~GS_SelectLevel()
{
}

void GS_SelectLevel::onRender()
{
	// Hintergrundbild rendern
	p_background->bind();
	glBegin(GL_QUADS);
	glColor3d(1.0, 1.0, 1.0);
	glTexCoord2i(0, 0);
	glVertex2i(0, 0);
	glTexCoord2i(640, 0);
	glVertex2i(640, 0);
	glTexCoord2i(640, 480);
	glVertex2i(640, 480);
	glTexCoord2i(0, 480);
	glVertex2i(0, 480);
	glEnd();
	p_background->unbind();

	int status = 0;
	if(p_currentLevel)
	{
		status = getLevelStatus(currentLevel);

		// Level-Vorschau rendern
		glEnable(GL_SCISSOR_TEST);
		glScissor(280, 480 - 60 - 200, 320, 200);
		glPushMatrix();
		glTranslated(280.0, 60.0, 0.0);
		glScaled(0.5, 0.5, 1.0);
		p_currentLevel->render();
		glPopMatrix();
		glDisable(GL_SCISSOR_TEST);

		// Name des Levels schreiben
		Font* p_font = GUI::inst().getFont();
		Vec2i dim;
		std::string title = localizeString(p_currentLevel->getTitle());
		char text[256] = "";
		sprintf(text, "%02d - %s", currentLevel + 1, status ? title.c_str() : "???");
		p_font->measureText(text, &dim, 0);
		p_font->renderText(text, Vec2i(440 - dim.x / 2, 270), Vec4d(1.0));
	}
	else
	{
		glBegin(GL_QUADS);
		glColor4d(0.0, 0.0, 0.0, 0.75);
		glVertex2i(280, 60);
		glVertex2i(280 + 320, 60);
		glVertex2i(280 + 320, 60 + 200);
		glVertex2i(280, 60 + 200);
		glEnd();
	}

	glDisable(GL_LINE_SMOOTH);
	glLineWidth(1.0f);
	glBegin(GL_LINE_LOOP);
	glColor4d(0.0, 0.0, 0.0, 0.5);
	glVertex2i(280, 60);
	glVertex2i(280 + 320, 60);
	glVertex2i(280 + 320, 60 + 200);
	glVertex2i(280, 60 + 200);
	glEnd();

	Font* p_font = GUI::inst().getFont();

	if(p_currentLevel)
	{
		if(status == 0 || status == -1)
		{
			// Level abdunkeln
			glBegin(GL_QUADS);
			glColor4d(0.0, 0.0, 0.0, 0.9);
			glVertex2i(280, 60);
			glVertex2i(280 + 320, 60);
			glVertex2i(280 + 320, 60 + 200);
			glVertex2i(280, 60 + 200);
			glEnd();

			std::string text;
			if(status == 0) text = localizeString("$LS_LEVEL_LOCKED");
			else if(status == -1) text = localizeString("$LS_BONUS_LEVEL_LOCKED");

			Vec2i dim;
			p_font->measureText(text, &dim, 0);
			p_font->renderText(text, Vec2i(280 + 160, 60 + 100) - dim / 2, Vec4d(1.0));

			status = 0;
		}

		Vec2i positionOnTexture(status * 40, 60);
		engine.renderSprite(p_misc, Vec2i(280 + 320 - 20, 60 + 200 - 20), positionOnTexture, Vec2i(39, 39), Vec4d(1.0));
	}

	glBegin(GL_QUADS);
	glColor4d(0.0, 0.0, 0.0, 0.5);
	glVertex2i(40, 240);
	glVertex2i(260, 240);
	glVertex2i(260, 260);
	glVertex2i(40, 260);
	glEnd();

	if(p_currentCampaign)
	{
		uint n = ProgressDB::inst().getNumLevelsCompleted(p_currentCampaign->getFilename());
		double p = static_cast<double>(n) / static_cast<double>(p_currentCampaign->getLevels().size());
		int pi = static_cast<int>(p * 220.0);

		if(n)
		{
			double r = 1.0 - p;
			double g = p;
			r = max(0.2, r);
			g = max(0.2, g);

			glBegin(GL_QUADS);
			glColor4d(r, g, 0.0, 0.9);
			glVertex2i(40 + pi, 240);
			glVertex2i(40 + pi, 260);
			glColor4d(r, g, 0.0, 0.5);
			glVertex2i(40, 260);
			glVertex2i(40, 240);
			glEnd();
		}

		Vec2i dim;
		char text[256] = "";
		sprintf(text, "%d/%d", n, static_cast<int>(p_currentCampaign->getLevels().size()));
		p_font->measureText(text, &dim, 0);
		p_font->renderText(text, Vec2i(150, 249) - dim / 2, Vec4d(1.0));
	}

	glBegin(GL_LINE_LOOP);
	glColor4d(0.0, 0.0, 0.0, 0.5);
	glVertex2i(40, 240);
	glVertex2i(260, 240);
	glVertex2i(260, 260);
	glVertex2i(40, 260);
	glEnd();

	glEnable(GL_LINE_SMOOTH);
}

void GS_SelectLevel::onUpdate()
{
	if(p_currentLevel)
	{
		// Level bewegen
		p_currentLevel->update();
	}

	if(engine.wasKeyPressed(SDLK_ESCAPE))
	{
		handleClick(gui["SelectLevel.Quit"]);
	}
	else if(engine.wasKeyPressed(SDLK_F7) &&
			(engine.isKeyDown(SDLK_LSHIFT) || engine.isKeyDown(SDLK_RSHIFT)))
	{
		if(p_currentCampaign)
		{
			// alle Levels der Kampagne freischalten
			for(uint i = 0; i < p_currentCampaign->getLevels().size(); i++)
			{
				ProgressDB::inst().setLevelCompleted(p_currentCampaign->getFilename(), i);
			}

			ProgressDB::inst().save();

			static_cast<GUI_Button*>(gui["SelectLevel.PlayLevel"])->activate();
		}
	}
}

void GS_SelectLevel::onEnter(const ParameterBlock& context)
{
	// Bilder laden
	p_background = Manager<Texture>::inst().request("selectlevel.png");
	p_misc = Manager<Texture>::inst().request("misc.png");

	// Menü erzeugen
	gui.getRoot()->load("selectlevel.xml");

	static_cast<GUI_ListBox*>(gui["SelectLevel.Campaigns"])->connectChanged(this, &GS_SelectLevel::handleClick);
	static_cast<GUI_Button*>(gui["SelectLevel.FirstLevel"])->connectClicked(this, &GS_SelectLevel::handleClick);
	static_cast<GUI_Button*>(gui["SelectLevel.PreviousLevel"])->connectClicked(this, &GS_SelectLevel::handleClick);
	static_cast<GUI_Button*>(gui["SelectLevel.NextLevel"])->connectClicked(this, &GS_SelectLevel::handleClick);
	static_cast<GUI_Button*>(gui["SelectLevel.NextLevelToDo"])->connectClicked(this, &GS_SelectLevel::handleClick);
	static_cast<GUI_Button*>(gui["SelectLevel.LastLevel"])->connectClicked(this, &GS_SelectLevel::handleClick);
	static_cast<GUI_Button*>(gui["SelectLevel.PlayLevel"])->connectClicked(this, &GS_SelectLevel::handleClick);
	static_cast<GUI_Button*>(gui["SelectLevel.Quit"])->connectClicked(this, &GS_SelectLevel::handleClick);

	// Kampagnen aufzählen
	FileSystem& fs = FileSystem::inst();
	GUI_ListBox* p_listBox = static_cast<GUI_ListBox*>(gui["SelectLevel.Campaigns"]);
	p_listBox->clear();
	campaigns.clear();
	std::list<std::string> files = fs.listDirectory(FileSystem::inst().getAppHomeDirectory() + "levels/campaigns");
	for(std::list<std::string>::const_iterator i = files.begin(); i != files.end(); ++i)
	{
		std::string ext = getFilenameExtension(*i);
		if(ext == "zip")
		{
			if(fs.fileExists(FileSystem::inst().getAppHomeDirectory() + "levels/campaigns/" + *i + "/campaign.xml"))
			{
				// Kampagne laden
				Campaign* p_campaign = new Campaign;
				if(p_campaign->load(FileSystem::inst().getAppHomeDirectory() + "levels/campaigns/" + *i))
				{
					GUI_ListBox::ListItem item(p_campaign->getTitle(), 0);
					p_listBox->addItem(item);
					campaigns.push_back(p_campaign);
				}
				else delete p_campaign;
			}
		}
	}

	p_listBox->setSelection(0);
}

void GS_SelectLevel::onLeave(const ParameterBlock& context)
{
	// Bilder löschen
	p_background->release();
	p_misc->release();
	p_background = 0;
	p_misc = 0;

	// Kampagnen löschen
	for(uint i = 0; i < campaigns.size(); i++) delete campaigns[i];
	campaigns.clear();
	p_currentCampaign = 0;

	// Level löschen
	delete p_currentLevel;
	p_currentLevel = 0;
	currentLevel = 0;

	// Menü löschen
	delete gui["SelectLevel"];
}

void GS_SelectLevel::onGetFocus()
{
	engine.playMusic("menu.ogg");

	gui["SelectLevel"]->focus();
	loadLevel();
}

void GS_SelectLevel::onLoseFocus()
{
	gui["SelectLevel"]->hide();
}

void GS_SelectLevel::handleClick(GUI_Element* p_element)
{
	const std::string& name = p_element->getFullName();
	if(name == "SelectLevel.Campaigns")
	{
		// Es wurde eine Kampagne ausgewählt. Beschreibungstext anzeigen!
		static_cast<GUI_Button*>(gui["SelectLevel.PlayLevel"])->deactivate();
		int i = static_cast<GUI_ListBox*>(p_element)->getSelection();
		currentLevel = 0;
		if(i != -1) p_currentCampaign = campaigns[i];
		else
		{
			p_currentCampaign = 0;
			delete p_currentLevel;
			p_currentLevel = 0;
		}

		std::string desc = "§de:Keine Kampagne ausgewählt.§en:No campaign selected.";
		if(p_currentCampaign) desc = p_currentCampaign->getDescription();
		static_cast<GUI_StaticText*>(gui["SelectLevel.CampaignDescription"])->setText(desc);

		loadLevel();
	}
	else if(name == "SelectLevel.FirstLevel")
	{
		if(currentLevel != 0)
		{
			currentLevel = 0;
			loadLevel();
		}
	}
	else if(name == "SelectLevel.PreviousLevel")
	{
		if(currentLevel > 0)
		{
			currentLevel--;
			loadLevel();
		}
	}
	else if(name == "SelectLevel.NextLevel")
	{
		if(p_currentCampaign)
		{
			if(currentLevel < p_currentCampaign->getLevels().size() - 1)
			{
				currentLevel++;
				loadLevel();
			}
		}
	}
	else if(name == "SelectLevel.NextLevelToDo")
	{
		if(p_currentCampaign)
		{
			// nächsten Level suchen, der noch nicht geschafft wurde
			uint oldLevel = currentLevel;
			uint numLevels = static_cast<uint>(p_currentCampaign->getLevels().size());
			uint i = currentLevel;
			while(true)
			{
				i++;
				if(i >= numLevels) i = 0;
				if(i == currentLevel) break;

				if(getLevelStatus(i) == 1)
				{
					currentLevel = i;
					break;
				}
			}

			if(oldLevel != currentLevel)
			{
				loadLevel();
			}
		}
	}
	else if(name == "SelectLevel.LastLevel")
	{
		if(p_currentCampaign)
		{
			uint lastLevel = static_cast<uint>(p_currentCampaign->getLevels().size()) - 1;
			if(currentLevel != lastLevel)
			{
				currentLevel = lastLevel;
				loadLevel();
			}
		}
	}
	else if(name == "SelectLevel.PlayLevel")
	{
		if(p_currentCampaign)
		{
			if(p_currentLevel)
			{
				delete p_currentLevel;
				p_currentLevel = 0;
			}

			ParameterBlock p;
			p.set("selectLevel", this);
			p.set("campaign", p_currentCampaign);
			p.set("levelNumber", currentLevel);
			engine.pushGameState("GS_Game", p);
			engine.crossfade(new CF_Cube, 0.85);
		}
	}
	else if(name == "SelectLevel.Quit")
	{
		engine.crossfade(new CF_Star, 0.85);
		engine.popGameState();
	}
}

void GS_SelectLevel::setCurrentLevel(uint currentLevel)
{
	if(!p_currentCampaign) return;
	if(currentLevel >= p_currentCampaign->getLevels().size()) return;

	this->currentLevel = currentLevel;
}

void GS_SelectLevel::loadLevel()
{
	updateNote();

	if(!p_currentCampaign) return;

	Level* p_oldLevel = p_currentLevel;
	if(p_oldLevel)
	{
		p_oldLevel->clean();
		p_oldLevel->removeOldObjects();
	}

	p_currentLevel = new Level;
	p_currentLevel->setInPreview(true);
	char levelName[256] = "";
	sprintf(levelName, "level_%d.xml", currentLevel + 1);
	p_currentLevel->load(p_currentCampaign->getFilename() + pw + "/" + levelName);

	if(p_oldLevel) delete p_oldLevel;

	int status = getLevelStatus(currentLevel);
	GUI_Button* p_button = static_cast<GUI_Button*>(gui["SelectLevel.PlayLevel"]);
	if(status != 0 && status != -1) p_button->activate();
	else p_button->deactivate();
}

int GS_SelectLevel::getLevelStatus(uint level)
{
	if(!p_currentCampaign) return -2;

	// Wurde der Level geschafft?
	ProgressDB& db = ProgressDB::inst();
	const std::string& campaign = p_currentCampaign->getFilename();
	if(db.wasLevelCompleted(campaign, level)) return 2;

	// Ist es der letzte Level, und hat die Kampagne einen Bonus-Level?
	uint numLevelsInCampaign = static_cast<uint>(p_currentCampaign->getLevels().size());
	if(level == numLevelsInCampaign - 1 && p_currentCampaign->hasBonusLevel())
	{
		// Der Level ist nur dann freigeschaltet, wenn alle anderen geschafft sind.
		if(db.getNumLevelsCompleted(campaign) == numLevelsInCampaign - 1) return 1;
		else return -1;
	}

	// Ist der Level schon freigeschaltet?
	uint numLevelsCompleted = db.getNumLevelsCompleted(campaign);
	uint numLevelsUnlocked = p_currentCampaign->getNumUnlockedLevels() + numLevelsCompleted;
	if(level < numLevelsUnlocked) return 1;

	// Der Level ist noch nicht freigeschaltet.
	return 0;
}

void GS_SelectLevel::updateNote()
{
	GUI_StaticText* p_note = static_cast<GUI_StaticText*>(gui["SelectLevel.Note"]);

	std::string text;
	if(p_currentCampaign)
	{
		std::ostringstream str;

		// Gibt es mehr als einen Level, der gespielt werden kann, aber noch nicht geschafft wurde?
		uint numLevelsPlayableButUnsolved = 0;
		for(uint level = 0; level < static_cast<uint>(p_currentCampaign->getLevels().size()); level++)
		{
			int status = getLevelStatus(level);
			if(status == 1)
			{
				if(numLevelsPlayableButUnsolved) str << ", " << level + 1;
				else str << level + 1;
				numLevelsPlayableButUnsolved++;
			}
		}

		if(numLevelsPlayableButUnsolved > 1) text = localizeString("$LS_NOTE") + "\n" + str.str();
		else text = "";
	}

	p_note->setText(text);
}