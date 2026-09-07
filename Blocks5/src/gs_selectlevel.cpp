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
#include "transfer.h"

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
	// render the background image
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

		// render the level preview
		glEnable(GL_SCISSOR_TEST);
		glScissor(280, 480 - 60 - 200, 320, 200);
		glPushMatrix();
		glTranslated(280.0, 60.0, 0.0);
		glScaled(0.5, 0.5, 1.0);
		p_currentLevel->render();
		glPopMatrix();
		glDisable(GL_SCISSOR_TEST);

		// write the level's name
		Font* p_font = GUI::inst().getFont();
		Vec2i dim;
		const std::string title = localizeString(p_currentLevel->getTitle());
		const std::string shown = status ? title : std::string("???");
		// As wide as the preview above it (the glScissor above), because the
		// name sits centred under it. Anything wider runs left into the
		// description and right off the picture: renderText() clips nothing.
		const int captionWidth = 320;

		// Only the title is shortened. The number and the filename are what
		// tells two levels of the same name apart, and they therefore stay
		// whole - how much is left for the title is what the same caption
		// with an empty one says.
		const bool single = p_currentCampaign->isSingleLevels();
		const std::string member = single
			? p_currentCampaign->getLevels()[currentLevel].member : std::string();
		Vec2i frameDim;
		p_font->measureText(single ? formatSingleLevelCaption(std::string(), member)
								   : formatLevelCaption(currentLevel + 1, std::string()),
							&frameDim, 0);
		const std::string fitted = p_font->fitText(shown, captionWidth - frameDim.x);

		// And once more over the whole caption, in case the frame alone is
		// already too wide: then the filename does get cut after all.
		const std::string caption = p_font->fitText(
			single ? formatSingleLevelCaption(fitted, member)
				   : formatLevelCaption(currentLevel + 1, fitted), captionWidth);
		p_font->measureText(caption, &dim, 0);
		p_font->renderText(caption, Vec2i(440 - dim.x / 2, 270), Vec4d(1.0));
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
			// darken the level
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

	// The bar counts completed levels. The single levels carry no progress,
	// and there the empty frame goes too - a bar that can never move reads as
	// a fault. The label above it disappears with it, in handleClick().
	if(p_currentCampaign && !p_currentCampaign->isSingleLevels())
	{
		glBegin(GL_QUADS);
		glColor4d(0.0, 0.0, 0.0, 0.5);
		glVertex2i(40, 240);
		glVertex2i(260, 240);
		glVertex2i(260, 260);
		glVertex2i(40, 260);
		glEnd();

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

		glBegin(GL_LINE_LOOP);
		glColor4d(0.0, 0.0, 0.0, 0.5);
		glVertex2i(40, 240);
		glVertex2i(260, 240);
		glVertex2i(260, 260);
		glVertex2i(40, 260);
		glEnd();
	}

	glEnable(GL_LINE_SMOOTH);
}

void GS_SelectLevel::onUpdate()
{
	if(p_currentLevel)
	{
		// move the level
		p_currentLevel->update();
	}

	const bool shift = engine.isKeyDown(SDLK_LSHIFT) || engine.isKeyDown(SDLK_RSHIFT);

	// While the campaign list holds the focus, the four keys it evaluates
	// itself belong to it - up, down, Home and End. The rest always operates
	// the dialog: the list does not know left and right at all, and forwards
	// Return anyway for want of a submit button.
	const bool listHasKeys = gui["SelectLevel.Campaigns"]->isFocusedIndirectly();

	if(engine.wasKeyPressed(SDLK_ESCAPE))
	{
		handleClick(gui["SelectLevel.Quit"]);
	}
	else if(engine.wasKeyPressed(SDLK_F7) && shift)
	{
		if(p_currentCampaign && !p_currentCampaign->isSingleLevels())
		{
			// unlock every level of the campaign
			for(uint i = 0; i < p_currentCampaign->getLevels().size(); i++)
			{
				ProgressDB::inst().setLevelCompleted(p_currentCampaign->getFilename(), i);
			}

			ProgressDB::inst().save();

			static_cast<GUI_Button*>(gui["SelectLevel.PlayLevel"])->activate();
		}
	}
	else if(engine.wasKeyPressed(SDLK_LEFT))
	{
		pressButton(gui["SelectLevel.PreviousLevel"]);
	}
	else if(engine.wasKeyPressed(SDLK_RIGHT))
	{
		if(shift) pressButton(gui["SelectLevel.NextLevelToDo"]);
		else      pressButton(gui["SelectLevel.NextLevel"]);
	}
	else if(engine.wasKeyPressed(SDLK_RETURN) || engine.wasKeyPressed(SDLK_KP_ENTER))
	{
		pressButton(gui["SelectLevel.PlayLevel"]);
	}
	else if(!listHasKeys)
	{
		if(engine.wasKeyPressed(SDLK_HOME)) pressButton(gui["SelectLevel.FirstLevel"]);
		else if(engine.wasKeyPressed(SDLK_END)) pressButton(gui["SelectLevel.LastLevel"]);
		else if(engine.wasKeyPressed(SDLK_UP)) selectCampaign(-1);
		else if(engine.wasKeyPressed(SDLK_DOWN)) selectCampaign(1);
	}
}

// What the keyboard triggers must have the same limits as the mouse: a locked
// level cannot be played with Return either, and there is no "next to do"
// button for the single levels. click() checks neither - with the mouse the
// GUI catches it beforehand.
void GS_SelectLevel::pressButton(GUI_Element* p_button)
{
	if(p_button->isActive() && p_button->isReallyVisible()) handleClick(p_button);
}

// One campaign forward or back. setSelection() fires changed(), which is
// what runs handleClick() afterwards and with it everything else.
void GS_SelectLevel::selectCampaign(int delta)
{
	const int count = static_cast<int>(campaigns.size());
	if(!count) return;

	GUI_ListBox* p_list = static_cast<GUI_ListBox*>(gui["SelectLevel.Campaigns"]);
	p_list->setSelection(clamp(p_list->getSelection() + delta, 0, count - 1));
}

void GS_SelectLevel::onEnter(const ParameterBlock& context)
{
	// load the images
	p_background = Manager<Texture>::inst().request("selectlevel.png");
	p_misc = Manager<Texture>::inst().request("misc.png");

	// create the menu
	gui.getRoot()->load("selectlevel.xml");

	static_cast<GUI_ListBox*>(gui["SelectLevel.Campaigns"])->connectChanged(this, &GS_SelectLevel::handleClick);
	static_cast<GUI_Button*>(gui["SelectLevel.FirstLevel"])->connectClicked(this, &GS_SelectLevel::handleClick);
	static_cast<GUI_Button*>(gui["SelectLevel.PreviousLevel"])->connectClicked(this, &GS_SelectLevel::handleClick);
	static_cast<GUI_Button*>(gui["SelectLevel.NextLevel"])->connectClicked(this, &GS_SelectLevel::handleClick);
	static_cast<GUI_Button*>(gui["SelectLevel.NextLevelToDo"])->connectClicked(this, &GS_SelectLevel::handleClick);
	static_cast<GUI_Button*>(gui["SelectLevel.LastLevel"])->connectClicked(this, &GS_SelectLevel::handleClick);
	static_cast<GUI_Button*>(gui["SelectLevel.PlayLevel"])->connectClicked(this, &GS_SelectLevel::handleClick);
	static_cast<GUI_Button*>(gui["SelectLevel.Quit"])->connectClicked(this, &GS_SelectLevel::handleClick);

	// enumerate the campaigns
	FileSystem& fs = FileSystem::inst();
	GUI_ListBox* p_listBox = static_cast<GUI_ListBox*>(gui["SelectLevel.Campaigns"]);
	p_listBox->clear();
	campaigns.clear();
	// Both roots: the shipped campaign sits with the game, imported and
	// self-made ones with the player. Transfer::list() already unites them,
	// in the right order and sorted.
	const std::vector<std::string> files(Transfer::list(Transfer::KIND_CAMPAIGN));
	for(std::vector<std::string>::const_iterator i = files.begin(); i != files.end(); ++i)
	{
		const std::string path(fs.resolveContentPath("levels/campaigns/" + *i));
		if(!fs.fileExists(path + "/campaign.xml")) continue;

		// load the campaign
		Campaign* p_campaign = new Campaign;
		if(p_campaign->load(path))
		{
			GUI_ListBox::ListItem item(p_campaign->getTitle(), 0);
			p_listBox->addItem(item);
			campaigns.push_back(p_campaign);
		}
		else delete p_campaign;
	}

	// And last the single levels from the level folder, if there are any: a
	// campaign that exists as no file. It comes last because the shipped
	// campaign is what a new player is looking for.
	Campaign* p_single = new Campaign;
	if(p_single->loadSingleLevels())
	{
		GUI_ListBox::ListItem item(p_single->getTitle(), 0);
		p_listBox->addItem(item);
		campaigns.push_back(p_single);
	}
	else delete p_single;

	p_listBox->setSelection(0);
}

void GS_SelectLevel::onLeave(const ParameterBlock& context)
{
	// release the images
	p_background->release();
	p_misc->release();
	p_background = 0;
	p_misc = 0;

	// delete the campaigns
	for(uint i = 0; i < campaigns.size(); i++) delete campaigns[i];
	campaigns.clear();
	p_currentCampaign = 0;

	// delete the level
	delete p_currentLevel;
	p_currentLevel = 0;
	currentLevel = 0;

	// delete the menu
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
		// A campaign has been selected; show the description text.
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

		std::string desc = "\xA7" "de:Keine Kampagne ausgew\xE4hlt.\xA7" "en:No campaign selected.";
		if(p_currentCampaign) desc = p_currentCampaign->getDescription();
		static_cast<GUI_StaticText*>(gui["SelectLevel.CampaignDescription"])->setText(desc);

		// "Next to do" looks for the next level not yet completed. Among the
		// single levels that is all of them, and the button would have no
		// business there; likewise the label above the progress bar, which
		// onRender() does not draw there in the first place.
		const bool single = p_currentCampaign && p_currentCampaign->isSingleLevels();
		if(single)
		{
			gui["SelectLevel.NextLevelToDo"]->hide();
			gui["SelectLevel.ProgressLabel"]->hide();
		}
		else
		{
			gui["SelectLevel.NextLevelToDo"]->show();
			gui["SelectLevel.ProgressLabel"]->show();
		}

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
			// look for the next level not yet completed
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

	// The entry knows for itself where it lies - in the campaign's archive
	// or as a loose file in the level folder. Composing "level_N.xml" by hand
	// would work only for the first case.
	p_currentLevel = new Level;
	p_currentLevel->setInPreview(true);
	p_currentLevel->load(p_currentCampaign->getLevels()[currentLevel].source());

	if(p_oldLevel) delete p_oldLevel;

	int status = getLevelStatus(currentLevel);
	GUI_Button* p_button = static_cast<GUI_Button*>(gui["SelectLevel.PlayLevel"]);
	if(status != 0 && status != -1) p_button->activate();
	else p_button->deactivate();
}

int GS_SelectLevel::getLevelStatus(uint level)
{
	if(!p_currentCampaign) return -2;

	// Has the level been completed?
	ProgressDB& db = ProgressDB::inst();
	const std::string& campaign = p_currentCampaign->getFilename();
	if(db.wasLevelCompleted(campaign, level)) return 2;

	// Is it the last level, and does the campaign have a bonus level?
	uint numLevelsInCampaign = static_cast<uint>(p_currentCampaign->getLevels().size());
	if(level == numLevelsInCampaign - 1 && p_currentCampaign->hasBonusLevel())
	{
		// The level is unlocked only once all the others are completed.
		if(db.getNumLevelsCompleted(campaign) == numLevelsInCampaign - 1) return 1;
		else return -1;
	}

	// Is the level unlocked already?
	uint numLevelsCompleted = db.getNumLevelsCompleted(campaign);
	uint numLevelsUnlocked = p_currentCampaign->getNumUnlockedLevels() + numLevelsCompleted;
	if(level < numLevelsUnlocked) return 1;

	// The level is not unlocked yet.
	return 0;
}

void GS_SelectLevel::updateNote()
{
	GUI_StaticText* p_note = static_cast<GUI_StaticText*>(gui["SelectLevel.Note"]);

	// The same reasoning as for the progress bar: the note lists what is
	// unlocked and still unsolved, and among the single levels that is every
	// one of them.
	std::string text;
	if(p_currentCampaign && !p_currentCampaign->isSingleLevels())
	{
		std::ostringstream str;

		// Is there more than one level that can be played but has not been completed yet?
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