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
	Renderer& renderer = Renderer::inst();
	const Vec4f white(1.0f, 1.0f, 1.0f, 1.0f);
	const Vec2f preview[4] = {Vec2f(280.0f, 60.0f), Vec2f(600.0f, 60.0f), Vec2f(600.0f, 260.0f), Vec2f(280.0f, 260.0f)};

	// render the background image
	const Vec2f screen[4] = {Vec2f(0.0f, 0.0f), Vec2f(640.0f, 0.0f), Vec2f(640.0f, 480.0f), Vec2f(0.0f, 480.0f)};
	renderer.setTexture(p_background->ref());
	renderer.quad(renderer.state(), screen, screen, white);

	int status = 0;
	if(p_currentLevel)
	{
		status = getLevelStatus(currentLevel);

		// render the level preview at half size, clipped to its frame
		{
			Renderer::ScissorScope clip(Vec2i(280, 60), Vec2i(320, 200));
			renderer.push();
			renderer.translate(280.0f, 60.0f);
			renderer.scale(0.5f, 0.5f);
			p_currentLevel->render();
			renderer.pop();
		}

		// write the level's name
		Font* p_font = GUI::inst().getFont();
		Vec2i dim;
		const std::string title = localizeString(p_currentLevel->getTitle());
		// -1 is the bonus level while it is still locked; it hides its name
		// like any other locked level, as the darkening below treats it.
		const std::string shown = status == 0 || status == -1 ? std::string("???") : title;
		// As wide as the preview above it (the clip above), because the
		// name sits centred under it. Anything wider runs left into the
		// description and right off the picture: renderText() clips nothing.
		const int captionWidth = 320;

		// Only the title is shortened: the number and the filename tell two
		// levels of the same name apart. The title gets whatever the same
		// caption with an empty title leaves.
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
		p_font->renderText(caption, Vec2i(440 - dim.x / 2, 270), Vec4f(1.0f));
	}
	else
	{
		renderer.rect(preview[0], preview[2], Vec4f(0.0f, 0.0f, 0.0f, 0.75f));
	}

	renderer.hairlineRect(preview[0], preview[2], Vec4f(0.0f, 0.0f, 0.0f, 0.5f));

	Font* p_font = GUI::inst().getFont();

	if(p_currentLevel)
	{
		if(status == 0 || status == -1)
		{
			// darken the level
			renderer.rect(preview[0], preview[2], Vec4f(0.0f, 0.0f, 0.0f, 0.9f));

			std::string text;
			if(status == 0) text = localizeString("$LS_LEVEL_LOCKED");
			else if(status == -1) text = localizeString("$LS_BONUS_LEVEL_LOCKED");

			Vec2i dim;
			p_font->measureText(text, &dim, 0);
			p_font->renderText(text, Vec2i(280 + 160, 60 + 100) - dim / 2, Vec4f(1.0f));

			status = 0;
		}

		Vec2i positionOnTexture(status * 40, 60);
		engine.renderSprite(p_misc, Vec2i(280 + 320 - 20, 60 + 200 - 20), positionOnTexture, Vec2i(39, 39), Vec4f(1.0f));
	}

	// The bar counts completed levels. The single levels carry no progress,
	// and there the empty frame goes too - a bar that can never move reads as
	// a fault. The label above it disappears with it, in handleClick().
	if(p_currentCampaign && !p_currentCampaign->isSingleLevels())
	{
		const Vec2f barMin(40.0f, 240.0f), barMax(260.0f, 260.0f);
		renderer.rect(barMin, barMax, Vec4f(0.0f, 0.0f, 0.0f, 0.5f));

		uint n = getNumLevelsCompleted();
		float p = static_cast<float>(n) / static_cast<float>(p_currentCampaign->getLevels().size());
		int pi = static_cast<int>(p * 220.0f);

		if(n)
		{
			float r = 1.0f - p;
			float g = p;
			r = max(0.2f, r);
			g = max(0.2f, g);

			// Corners from the moving end back to the fixed one, so the fade
			// runs from the bar's front to its start.
			const Vec4f front(r, g, 0.0f, 0.9f);
			const Vec4f back(r, g, 0.0f, 0.5f);
			const float right = static_cast<float>(40 + pi);
			const Vec2f corners[4] = {Vec2f(right, 240.0f), Vec2f(right, 260.0f), Vec2f(40.0f, 260.0f), Vec2f(40.0f, 240.0f)};
			const Vec4f colors[4] = {front, front, back, back};
			renderer.quad(corners, colors);
		}

		Vec2i dim;
		char text[256] = "";
		sprintf(text, "%d/%d", n, static_cast<int>(p_currentCampaign->getLevels().size()));
		p_font->measureText(text, &dim, 0);
		p_font->renderText(text, Vec2i(150, 249) - dim / 2, Vec4f(1.0f));

		renderer.hairlineRect(barMin, barMax, Vec4f(0.0f, 0.0f, 0.0f, 0.5f));
	}
}

void GS_SelectLevel::onUpdate()
{
	if(p_currentLevel)
	{
		// move the level
		p_currentLevel->update();
	}

	const bool shift = engine.isKeyDown(SDLK_LSHIFT) || engine.isKeyDown(SDLK_RSHIFT);
	const bool ctrl = engine.isKeyDown(SDLK_LCTRL) || engine.isKeyDown(SDLK_RCTRL);

	// While the campaign list holds the focus, the four keys it handles
	// itself - up, down, Home and End - are left to it. The rest always drive
	// the dialog: the list ignores left and right and forwards Return for
	// want of a submit button.
	const bool listHasKeys = gui["SelectLevel.Campaigns"]->isFocusedIndirectly();

	if(engine.wasKeyPressed(SDLK_ESCAPE))
	{
		handleClick(gui["SelectLevel.Quit"]);
	}
	// Ctrl+Shift+F7, one of the author's private chords, which all use a
	// function key: pre.js hands the game F1 to F24, and no browser reserves
	// those the way it reserves the letters.
	else if(engine.wasKeyPressed(SDLK_F7) && shift && ctrl)
	{
		if(p_currentCampaign && !p_currentCampaign->isSingleLevels())
		{
			// unlock every level of the campaign
			std::vector<std::pair<std::string, uint> > solved;
			for(uint i = 0; i < p_currentCampaign->getLevels().size(); i++)
			{
				solved.push_back(std::make_pair(p_currentCampaign->getFilename(), i));
			}

			ProgressDB::inst().markSolved(solved);
			refreshProgress();

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

// Keys get the same limits as the mouse: a locked level cannot be played with
// Return either, and the single levels have no "next to do". Neither
// handleClick() nor GUI_Button::click() checks that; for the mouse the GUI
// does.
void GS_SelectLevel::pressButton(GUI_Element* p_button)
{
	if(p_button->isActive() && p_button->isReallyVisible()) handleClick(p_button);
}

// One campaign forward or back. setSelection() fires changed(), which runs
// handleClick() and with it everything else.
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

	// build the menu
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
	bool shippedCampaign = false;
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
			if(Transfer::isBuiltIn(Transfer::KIND_CAMPAIGN, *i)) shippedCampaign = true;
		}
		else delete p_campaign;
	}

	// The game's own campaign is an archive in the game folder and a build
	// product, so a tree cloned but never packed lacks it and the list would
	// silently come up without it. A broken one reports itself through
	// Campaign::load; this covers only nothing shipped turning up at all.
	if(!shippedCampaign) engine.showToast(Engine::TOAST_ERROR, "$ERROR_NO_BUILT_IN_CAMPAIGN");

	// Last, the single levels from the level folder if there are any: a
	// campaign that exists as no file. Last because the shipped campaign is
	// what a new player is looking for.
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
	engine.playMusic("menu.ogg", 0.0f, true);

	// Here and not in onEnter(): coming back from a played level is a pop, and
	// popGameState() gives the state underneath the focus without entering it
	// again. Read there, a level just solved would still be shown as unsolved.
	refreshProgress();

	gui["SelectLevel"]->focus();
	loadLevel();
}

void GS_SelectLevel::refreshProgress()
{
	progress = ProgressDB::inst().query();
}

uint GS_SelectLevel::getNumLevelsCompleted() const
{
	if(!p_currentCampaign) return 0;

	const ProgressDB::Progress::const_iterator i =
		progress.find(ProgressDB::keyFor(p_currentCampaign->getFilename()));
	const uint completed = (i == progress.end()) ? 0 : static_cast<uint>(i->second.size());

	// Never more than the campaign holds: a merged database can carry levels
	// of a campaign that has since become shorter, or of another campaign of
	// the same name, and the bar would then run past its frame and read
	// "45/42".
	const uint total = static_cast<uint>(p_currentCampaign->getLevels().size());
	return min(completed, total);
}

bool GS_SelectLevel::wasLevelCompleted(uint level) const
{
	if(!p_currentCampaign) return false;

	const ProgressDB::Progress::const_iterator i =
		progress.find(ProgressDB::keyFor(p_currentCampaign->getFilename()));
	if(i == progress.end()) return false;

	return i->second.find(level) != i->second.end();
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

		// "Next to do" looks for the next level not yet completed, which among
		// the single levels is all of them, so there the button is hidden -
		// and so is the label above the progress bar, which onRender() does
		// not draw there either.
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
			engine.crossfade(new CF_Cube, 0.85f);
		}
	}
	else if(name == "SelectLevel.Quit")
	{
		engine.crossfade(new CF_Star, 0.85f);
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

	// The entry knows where it lies, in the campaign's archive or as a loose
	// file in the level folder; composing "level_N.xml" by hand would work
	// only for the first.
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
	if(wasLevelCompleted(level)) return 2;

	// Is it the last level, and does the campaign have a bonus level?
	uint numLevelsInCampaign = static_cast<uint>(p_currentCampaign->getLevels().size());
	if(level == numLevelsInCampaign - 1 && p_currentCampaign->hasBonusLevel())
	{
		// The level is unlocked only once all the others are completed.
		if(getNumLevelsCompleted() == numLevelsInCampaign - 1) return 1;
		else return -1;
	}

	// Is the level unlocked already?
	uint numLevelsCompleted = getNumLevelsCompleted();
	uint numLevelsUnlocked = p_currentCampaign->getNumUnlockedLevels() + numLevelsCompleted;
	if(level < numLevelsUnlocked) return 1;

	// The level is not unlocked yet.
	return 0;
}

void GS_SelectLevel::updateNote()
{
	GUI_StaticText* p_note = static_cast<GUI_StaticText*>(gui["SelectLevel.Note"]);

	// Not for the single levels, as with the progress bar: the note lists
	// what is unlocked and still unsolved, which there is every level.
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