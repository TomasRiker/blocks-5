#include "pch.h"
#include "filesystem.h"
#include "gs_game.h"
#include "gs_selectlevel.h"
#include "gui_all.h"
#include "level.h"
#include "presets.h"
#include "player.h"
#include "exit.h"
#include "cf_all.h"
#include "u_crt.h"

namespace
{
	// On a restart the game jumps from the current state to the start with
	// nothing in between. With the CRT filter in front of it that may look
	// like a video recorder in rewind - which jumps the same way and nobody
	// minds. Without the filter it would be picture noise for no reason, and
	// it stays with the slices.
	void crossfadeRestart(Engine& engine)
	{
		if(engine.getEffectiveUpscaler() == &engine.getCrt()) engine.crossfade(new CF_Rewind, 1.5);
		else engine.crossfade(new CF_Slices, 0.85);
	}

	// Light up a HUD icon exactly the way an object in the level does
	// (object.cpp): the same image once more over itself, additively, with the
	// decaying strength as the colour.
	void renderIconFlash(Level& level,
						 const char* p_preset,
						 const Vec2i& position,
						 uint index)
	{
		const double f = level.getHudIconFlash(index);
		if(f <= 0.0) return;

		Engine& engine = Engine::inst();
		engine.setBlendFunc(GL_SRC_ALPHA, GL_ONE, GL_ONE, GL_ONE);
		level.getPresets()->renderPreset(p_preset, position, Vec4d(f, f, f, 1.0));
		engine.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
	}
}
#include "streamedsound.h"
#include "texture.h"
#include "options.h"
#include "help.h"
#include "campaign.h"
#include "transfer.h"
#include "progressdb.h"
#include "hotel.h"

class GameGUI : public GUI_Element, public sigslot::has_slots<>
{
public:
	GameGUI(GS_Game& game) : GUI_Element("Game", 0, Vec2i(0, 0), Vec2i(640, 480)), game(game)
	{
		load("game.xml");

		static_cast<GUI_Button*>(getChild("ShowMenu"))->connectClicked(this, &GameGUI::handleClick);

		static_cast<GUI_Button*>(getChild("MenuPane.Menu.Continue"))->connectClicked(this, &GameGUI::handleClick);
		static_cast<GUI_Button*>(getChild("MenuPane.Menu.Restart"))->connectClicked(this, &GameGUI::handleClick);
		static_cast<GUI_Button*>(getChild("MenuPane.Menu.RestartFromHotel"))->connectClicked(this, &GameGUI::handleClick);
		static_cast<GUI_Button*>(getChild("MenuPane.Menu.Options"))->connectClicked(this, &GameGUI::handleClick);
		static_cast<GUI_Button*>(getChild("MenuPane.Menu.Help"))->connectClicked(this, &GameGUI::handleClick);
		static_cast<GUI_Button*>(getChild("MenuPane.Menu.Quit"))->connectClicked(this, &GameGUI::handleClick);

		static_cast<GUI_Button*>(getChild("MenuPane.Menu.Quit"))->setTitle(game.cameFromEditor ? "$G_MENU_RETURN_TO_LEVEL_EDITOR" : "$G_MENU_RETURN_TO_LEVEL_SELECTION");
		if(game.cameFromEditor) getChild("MenuPane.Quit")->hide();
		else
		{
			getChild("MenuPane.Quit")->show();
			static_cast<GUI_Button*>(getChild("MenuPane.Quit"))->connectClicked(this, &GameGUI::handleClick);
		}

		p_options = new Options(this);
		p_help = new Help(this);
	}

	~GameGUI()
	{
		delete p_options;
		delete p_help;
	}

	void onUpdate()
	{
		if(getChild("MenuPane")->isVisible()) game.showCursor = 200;
	}

	void onMouseDown(const Vec2i& position,
					 int buttons)
	{
		game.showCursor = 200;

		// Did the click land on a player?
		Vec2i c = game.engine.getCursorPosition() / 16;
		std::vector<Object*> objects = game.p_level->getObjectsAt(c);
		for(std::vector<Object*>::const_iterator i = objects.begin(); i != objects.end(); ++i)
		{
			Object* p_obj = *i;
			if(p_obj->getType() == "Player")
			{
				static_cast<Player*>(p_obj)->activate();
			}
		}
	}

	void onMouseMove(const Vec2i& movement,
					 const Vec2i& position,
					 int buttons)
	{
		if(game.ignoreNextCursorMovement) game.ignoreNextCursorMovement = false;
		else
		{
			game.showCursor = 200;
		}
	}

	void onKeyEvent(const SDL_KeyboardEvent& event)
	{
		// Only a newly pressed key counts. A repeat is not a second command -
		// without that, a held Escape opens and closes the menu over and over.
		const bool pressed = (event.type == SDL_KEYDOWN) && !GUI::inst().isKeyRepeat();

		if(GUI::inst()["Game.MenuPane"]->isVisible())
		{
			// Escape closes the menu again, as in the level editor. But only
			// the menu itself: with options or help standing over it,
			// "MenuPane.Menu" is hidden and the key belongs to the dialog.
			if(pressed && event.keysym.sym == SDLK_ESCAPE &&
			   getChild("MenuPane.Menu")->isReallyVisible())
			{
				handleClick(getChild("MenuPane.Menu.Continue"));
			}
			return;
		}

		if(event.type == SDL_KEYUP && event.keysym.sym == SDLK_TAB) game.switchTimer = 0;
		if(!pressed) return;

		// An open hint note gets Return and Escape first. It is as tall as the
		// play area and covers it, and without this the player would have to
		// walk off the field to see anything again. With nothing to close,
		// dismissDisplay() reports false and the key goes its usual way -
		// Escape therefore on into the game menu.
		if(event.keysym.sym == SDLK_ESCAPE ||
		   event.keysym.sym == SDLK_RETURN ||
		   event.keysym.sym == SDLK_KP_ENTER)
		{
			if(game.p_level && game.p_level->dismissDisplay()) return;
		}

		switch(event.keysym.sym)
		{
		case SDLK_ESCAPE:
			handleClick(getChild("ShowMenu"));
			break;
		}
	}

	void handleClick(GUI_Element* p_element)
	{
		game.showCursor = 200;

		const std::string& name = p_element->getFullName();
		if(name == "Game.ShowMenu")
		{
			getChild("MenuPane.Menu")->focus();
			getChild("MenuPane.Menu.Continue")->activate();
			if(game.p_saveGame) getChild("MenuPane.Menu.RestartFromHotel")->activate();
			else getChild("MenuPane.Menu.RestartFromHotel")->deactivate();
		}

		if(name == "Game.MenuPane.Menu.Continue")
		{
			getChild("MenuPane")->hide();
			focus();
		}
		else if(name == "Game.MenuPane.Menu.Restart")
		{
			// restore the level
			Level* p_oldLevel = game.p_level;
			p_oldLevel->clean();
			p_oldLevel->removeOldObjects();
			game.p_level = new Level;
			game.p_level->load(game.p_originalLevel);
			delete p_oldLevel;
			crossfadeRestart(game.engine);
			getChild("MenuPane")->hide();
			focus();
			game.leaveCountDown = 50;
		}
		else if(name == "Game.MenuPane.Menu.RestartFromHotel")
		{
			// restore the level
			Level* p_oldLevel = game.p_level;
			p_oldLevel->clean();
			p_oldLevel->removeOldObjects();
			game.p_level = new Level;
			game.p_level->load(game.p_saveGame);
			delete p_oldLevel;
			crossfadeRestart(game.engine);
			getChild("MenuPane")->hide();
			focus();
			game.leaveCountDown = 50;
		}
		else if(name == "Game.MenuPane.Menu.Options")
		{
			getChild("MenuPane.Menu")->hide();
			p_options->show(getChild("MenuPane.Menu"));
		}
		else if(name == "Game.MenuPane.Menu.Help")
		{
			getChild("MenuPane.Menu")->hide();
			p_help->show(getChild("MenuPane.Menu"));
		}
		else if(name == "Game.MenuPane.Menu.Quit")
		{
			if(game.cameFromEditor) game.engine.crossfade(new CF_Mosaic, 0.85);
			else game.engine.crossfade(new CF_Cube, 0.85);
			game.engine.popGameState();
		}
		else if(name == "Game.MenuPane.Quit")
		{
			SDL_Event event;
			event.type = SDL_QUIT;
			SDL_PushEvent(&event);
		}
	}

private:
	GS_Game& game;
	Options* p_options;
	Help* p_help;
};

GS_Game::GS_Game() : GameState("GS_Game"), engine(Engine::inst()), showCursor(0), ignoreNextCursorMovement(false)
{
}

GS_Game::~GS_Game()
{
}

void GS_Game::onRender()
{
	// render the level
	p_level->render();

	// render the panel
	p_level->getBackground()->bind();
	glBegin(GL_QUADS);
	glColor4d(1.0, 1.0, 1.0, 1.0);
	glTexCoord2i(0, 480);
	glVertex2i(0, 400);
	glTexCoord2i(640, 480);
	glVertex2i(640, 400);
	glTexCoord2i(640, 560);
	glVertex2i(640, 480);
	glTexCoord2i(0, 560);
	glVertex2i(0, 480);
	glEnd();
	p_level->getBackground()->unbind();

	Font* p_font = gui.getFont();

	// render the data
	char text[256] = "";
	uint nd = p_level->getNumDiamondsCollected();
	p_level->getPresets()->renderPreset("Diamond", Vec2i(32, 416));
	renderIconFlash(*p_level, "Diamond", Vec2i(32, 416), 1);
	sprintf(text, "%d/%d", nd, p_level->getNumDiamondsNeeded());
	double alpha;
	if(nd >= p_level->getNumDiamondsNeeded()) alpha = 0.7 + 0.3 * sin(static_cast<double>(p_level->counter) * 0.4);
	else alpha = 1.0;
	p_font->renderText(text, Vec2i(66, 416), Vec4d(1.0, 1.0, 1.0, alpha));

	Player* p_player = p_level->getActivePlayer();
	if(p_player)
	{
		p_level->getPresets()->renderPreset("Bomb", Vec2i(32, 448));
		renderIconFlash(*p_level, "Bomb", Vec2i(32, 448), 0);
		sprintf(text, "%d", p_player->getInventory(0));
		p_font->renderText(text, Vec2i(66, 448), Vec4d(1.0, 1.0, 1.0, 1.0));

		double c = static_cast<double>(p_player->getContamination());
		if(c > 0.0)
		{
			Engine& engine = Engine::inst();
			double t = static_cast<double>(engine.getTime()) / 1000.0;
			double x = min(0.5, 0.002 * c);
			double f = c < 375.0 ? 2.0 : 4.0;
			double s = sin(6.282 * f * t);
			Vec4d color(1.0, 1.0 - x, 1.0 - x, 0.7 + 0.4 * s);
			double scaling = 1.0 + min(0.2, 0.002 * c) * s;
			engine.renderSprite(p_misc, Vec2i(168, 416), Vec2i(0, 0), Vec2i(48, 48), color, false, 0.0, scaling);
		}
	}

	// The level number belongs to the campaign. A trial run from the editor and
	// a single level have none; the single level names its file instead, to
	// keep three levels of the same name apart.
	const std::string title = localizeString(p_level->getTitle());

	// The space between the image at 168 (48 wide) and the menu button at 544,
	// centred on 384 - the button is the tighter side, hence
	// 2 * (544 - 8 - 384), with the eight as a gap to keep the caption from
	// touching it. The rest has to go: renderText() clips nothing and would
	// run across both.
	const int captionWidth = 304;

	const bool single = !cameFromEditor && p_currentCampaign
						&& p_currentCampaign->isSingleLevels();
	const bool numbered = !cameFromEditor && p_currentCampaign && !single;

	// Only the title is shortened, not the number and not the filename: those
	// two say which level this is.
	Vec2i frameDim(0, 0);
	if(single)        p_font->measureText(formatSingleLevelCaption(std::string(), levelFilename), &frameDim, 0);
	else if(numbered) p_font->measureText(formatLevelCaption(levelNumber + 1, std::string()), &frameDim, 0);
	const std::string fitted = p_font->fitText(title, captionWidth - frameDim.x);

	std::string caption = fitted;
	if(single)        caption = formatSingleLevelCaption(fitted, levelFilename);
	else if(numbered) caption = formatLevelCaption(levelNumber + 1, fitted);

	// And once more over the whole caption, in case the frame alone is already
	// too wide: then the filename does get cut after all.
	caption = p_font->fitText(caption, captionWidth);

	Vec2i dim;
	p_font->measureText(caption, &dim, 0);
	p_font->renderText(caption, Vec2i(384 - dim.x / 2, 432), Vec4d(1.0, 1.0, 1.0, 1.0));

	if(engine.isKeyDown(SDLK_f))
	{
		sprintf(text, "Frame: %d ms", engine.getFrameTime());
		p_font->renderText(text, Vec2i(10, 10), Vec4d(1.0, 1.0, 1.0, 0.5));
	}

	if(paused)
	{
		double t = 0.001 * engine.getTime();
		double r = 0.5 + 0.5 * sin(3.0 * t);
		double g = 0.5 + 0.5 * cos(4.27 * t);
		double b = 0.5 + 0.5 * sin(5.13 * t);
		double a = 0.7 + 0.3 * sin(6.26 * t);
		p_font->renderText("Pause", pausePosition, Vec4d(r, g, b, a));
	}
}

void GS_Game::onUpdate()
{
	if(switchTimer) switchTimer--;

	bool menuVisible = GUI::inst()["Game.MenuPane"]->isVisible();

	Engine& engine = Engine::inst();
	GameGUI& gameGUI = *(static_cast<GameGUI*>(GUI::inst()["Game"]));

	if(engine.wasActionPressed("$A_RESTART_LEVEL") ||
	   (p_level->isInMenu() && engine.wasKeyPressed(SDLK_TAB)))
	{
		gameGUI.handleClick(gameGUI["MenuPane.Menu.Restart"]);
	}
	else if(engine.wasActionPressed("$A_RESTART_FROM_HOTEL"))
	{
		if(p_saveGame) gameGUI.handleClick(gameGUI["MenuPane.Menu.RestartFromHotel"]);
	}

	// Any key and any click leave the pause, not only the pause key: anyone
	// who wants to play on reaches for the controls anyway, and after a switch
	// to another window - which pauses too - the click back into the game is
	// the natural gesture. The press is spent on that, and the rest of this
	// block falls away, or the pause key would switch straight back on what it
	// has just switched off.
	if(paused && (engine.wasAnyKeyPressed() || engine.wasAnyButtonPressed()))
	{
		paused = false;
	}
	else if(!menuVisible)
	{
		if(engine.wasActionPressed("$A_SWITCH_CHARACTER"))
		{
			if(!switchTimer)
			{
				// select the next player
				p_level->switchToNextPlayer();
				switchTimer = 20;
			}
		}
		else if(engine.wasActionPressed("$A_SAVE_IN_HOTEL"))
		{
			if(Hotel::p_hotelToSave)
			{
				Hotel::p_hotelToSave->onSave();
				delete p_saveGame;
				p_saveGame = p_level->save();
			}
		}
		else if(engine.wasActionPressed("$A_PAUSE"))
		{
			paused = !paused;
		}
	}

	// player dead?
	bool allDead = !Player::getNumInstances();
	if(allDead)
	{
		if(leaveCountDown) leaveCountDown--;

		if(leaveCountDown == 1)
		{
			static_cast<GUI_Button*>(gameGUI["ShowMenu"])->click();
			gameGUI["MenuPane.Menu.Continue"]->deactivate();
		}
	}

	// player contaminated?
	Player* p_player = p_level->getActivePlayer();
	if(p_player)
	{
		// Greater than zero and not merely non-zero: below zero means the
		// player has collected syringes in reserve and holds out longer. The
		// player is then not contaminated but better than clean - nothing
		// crackles, and no toxic gas is spread either. (The screen tint further
		// up has always had it that way.)
		int c = p_player->getContamination();
		if(c > 0)
		{
			if(c >= 50)
			{
				p_level->addToxic(0.05);
			}

			if(random(0, 2000 + c) >= 2000)
			{
				// play the Geiger counter sound
				Engine::inst().playSound("geiger.ogg", false, 0.2);
			}
		}
	}

	// level completed?
	if(p_level->finished)
	{
		// A single level belongs to no campaign: there is nothing to record,
		// and the next entry is a stranger's level rather than the next step.
		// Both therefore as with a trial run from the editor - afterwards back
		// to where the player came from.
		const bool ownLevel = cameFromEditor ||
							  (p_currentCampaign && p_currentCampaign->isSingleLevels());

		if(!ownLevel)
		{
			// record the progress
			std::vector<std::pair<std::string, uint> > solved;
			solved.push_back(std::make_pair(p_currentCampaign->getFilename(), levelNumber));

			if(!ProgressDB::inst().markSolved(solved))
			{
				// The old database is back in place, so nothing of the
				// player's is lost - but this level is not in it, and finding
				// that out at the next start would be worse than a red bar.
				engine.showToast(Engine::TOAST_ERROR, loadString("$TR_ERROR_PROGRESS_SAVE"));
			}
		}

		// next level, or back to the menu
		Vec2i targetIn = p_level->getExit()->getShownPositionInPixels() + Vec2i(8, 8);
		Vec2i targetOut;
		levelNumber++;
		int status;
		if(ownLevel) status = -3;
		else status = loadLevel();
		if(status == 1)
		{
			targetOut = p_level->getActivePlayer()->getShownPositionInPixels() + Vec2i(8, 8);
			Engine::inst().crossfade(new CF_Zoom(targetIn, targetOut), 3.0);
		}
		else
		{
			targetOut = Vec2i(320, 240);

			if(status == -1)
			{
				// The final level is done. The credits exist only for the
				// shipped campaign - every other one returns to the selection.
				//
				// The filename is what is asked about, not the path: the
				// shipped campaign lives in the game folder, an imported one in
				// the user directory, and shipped is exactly what
				// Transfer::isBuiltIn() takes it to be.
				if(Transfer::isBuiltIn(Transfer::KIND_CAMPAIGN,
									   FileSystem::inst().getPathFilename(p_currentCampaign->getFilename())))
				{
					// The game is over.
					Engine::inst().setGameState("GS_Credits");
					Engine::inst().crossfade(new CF_ColorBlend(Vec3d(0.0, 0.0, 0.0), 0.5), 2.0);
				}
				else
				{
					Engine::inst().popGameState();
					Engine::inst().crossfade(new CF_ColorBlend(Vec3d(0.0, 0.0, 0.0), 0.5), 2.0);
					if(p_selectLevel) p_selectLevel->setCurrentLevel(levelNumber - 1);
				}
			}
			else if(status == -2)
			{
				// The next level is the bonus level.
				Engine::inst().popGameState();
				Engine::inst().crossfade(new CF_Zoom(targetIn, targetOut), 3.0);
				if(p_selectLevel) p_selectLevel->setCurrentLevel(levelNumber);
			}
			else if(status == 0)
			{
				// error
				Engine::inst().crossfade(new CF_Zoom(targetIn, targetOut), 3.0);
				if(p_selectLevel) p_selectLevel->setCurrentLevel(levelNumber - 1);
			}
			else if(status == -3)
			{
				Engine::inst().popGameState();
				Engine::inst().crossfade(new CF_Zoom(targetIn, targetOut), 3.0);
			}
		}
	}

	if(menuVisible) paused = false;

	if(!paused && (!menuVisible || allDead))
	{
		// move the level
		p_level->update();
	}

	if(!menuVisible && showCursor)
	{
		showCursor--;
		if(!showCursor) ignoreNextCursorMovement = true;
	}

	SDL_ShowCursor(showCursor ? 1 : 0);

	if(paused)
	{
		pausePosition += 0.02 * 190.0 * pauseVelocity;

		if(pausePosition.x < 0.0 || pausePosition.x > 600.0)
		{
			pauseVelocity.x *= -1.0;
			pauseVelocity.y += random(-0.2, 0.2);
			pauseVelocity.normalize();
		}

		if(pausePosition.y < 0.0 || pausePosition.y > 382.0)
		{
			pauseVelocity.y *= -1.0;
			pauseVelocity.x += random(-0.2, 0.2);
			pauseVelocity.normalize();
		}

		pausePosition += 0.02 * 10.0 * pauseVelocity;
	}
}

void GS_Game::onEnter(const ParameterBlock& context)
{
	p_selectLevel = 0;
	if(context.has("selectLevel")) p_selectLevel = context.get<GS_SelectLevel*>("selectLevel");
	cameFromEditor = context.has("levelDocument");
	p_originalLevel = 0;
	p_saveGame = 0;
	levelFilename = "";
	paused = false;
	pausePosition = Vec2d(320.0, 200.0);
	const double r = random(0.0, 6.2832);
	pauseVelocity = Vec2d(sin(r), cos(r));

	// load the level
	p_level = new Level;
	if(cameFromEditor)
	{
		TiXmlDocument* p_doc = context.get<TiXmlDocument*>("levelDocument");
		p_level->load(p_doc);
		delete p_doc;

		// save the level
		p_originalLevel = p_level->save();

		// Play the music. A loose level names a file beside itself, or one of
		// the shipped campaign's tracks with "blocks:". A trial run from the
		// editor has no file path: the level stands in memory as a document,
		// and a track beside it can therefore only be in the player's level
		// folder.
		Engine::inst().playMusic(Campaign::resolveMusicPath(p_level->getMusicFilename(),
														   FileSystem::inst().getAppHomeDirectory() + "levels/"));
	}
	else
	{
		levelNumber = context.get<uint>("levelNumber");
		p_currentCampaign = context.get<Campaign*>("campaign");
		loadLevel();
	}

	// load the images
	p_misc = Manager<Texture>::inst().request("misc.png");

	leaveCountDown = 50;
	switchTimer = 0;

	// create the dialog
	new GameGUI(*this);
}

void GS_Game::onLeave(const ParameterBlock& context)
{
	// free the resources
	delete p_level;
	delete p_originalLevel;
	delete p_saveGame;
	p_misc->release();
	p_level = 0;
	p_originalLevel = 0;
	p_saveGame = 0;
	p_misc = 0;

	// delete the dialog
	delete gui["Game"];

	// stop the music
	Engine::inst().stopMusic();

	// tell the level select screen which level we are in
	if(p_selectLevel) p_selectLevel->setCurrentLevel(levelNumber);
}

void GS_Game::onGetFocus()
{
	gui["Game"]->focus();
	showCursor = 200;
}

void GS_Game::onLoseFocus()
{
	gui["Game"]->hide();
	SDL_ShowCursor(1);
}

void GS_Game::onAppLoseFocus()
{
	paused = true;
}

int GS_Game::loadLevel()
{
	delete p_saveGame;
	p_saveGame = 0;

	// Is this the last level, and does the campaign have a bonus level?
	if(levelNumber == p_currentCampaign->getLevels().size() - 1 &&
	   p_currentCampaign->hasBonusLevel())
	{
		// Were fewer levels completed than required?
		// Read afresh rather than kept: this runs once per loaded level, and the
		// database may have been imported or merged since the game started.
		const ProgressDB::Progress progress = ProgressDB::inst().query();
		const ProgressDB::Progress::const_iterator entry =
			progress.find(ProgressDB::keyFor(p_currentCampaign->getFilename()));
		const size_t total = p_currentCampaign->getLevels().size();
		const size_t completed = min((entry == progress.end()) ? size_t(0) : entry->second.size(),
									 total);

		// Clamped as in the level selection: a merged database can hold levels
		// of a campaign that has since become shorter, and the two must not
		// disagree about whether the bonus level is earned.
		if(completed < total - 1)
		{
			return -2;
		}
	}

	if(levelNumber >= p_currentCampaign->getLevels().size())
	{
		// That level ended the campaign.
		return -1;
	}

	// As in the selection: the entry knows its source, the archive or the
	// loose file.
	const Campaign::LevelRef& ref = p_currentCampaign->getLevels()[levelNumber];
	levelFilename = ref.member;

	Level* p_oldLevel = p_level;
	p_level = new Level;
	bool r = p_level->load(ref.source());
	if(r)
	{
		// save the level
		delete p_originalLevel;
		p_originalLevel = p_level->save();
	}

	delete p_oldLevel;

	// Play the music - from the campaign's archive, or from blocks.zip if the
	// level takes the track from there with "blocks:".
	Engine::inst().playMusic(Campaign::resolveMusicPath(p_level->getMusicFilename(), ref.sourceDir));

	return r ? 1 : 0;
}