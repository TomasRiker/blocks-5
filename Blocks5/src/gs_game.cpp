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
#include "streamedsound.h"
#include "texture.h"
#include "options.h"
#include "help.h"
#include "campaign.h"
#include "transfer.h"
#include "progressdb.h"

namespace
{
	// A restart jumps to the start with nothing in between. Under the CRT
	// filter that reads as a tape rewinding, which jumps the same way; without
	// it the tape noise would be a costume, so it gets the slices.
	void crossfadeRestart(Engine& engine)
	{
		if(engine.getUpscaler() == &engine.getCrt()) engine.crossfade(new CF_Rewind, 1.5f);
		else engine.crossfade(new CF_Slices, 0.85f);
	}

	// Light up a HUD icon exactly the way an object in the level does
	// (object.cpp): the same image once more over itself, additively, with the
	// decaying strength as the colour.
	void renderIconFlash(Level& level,
						 const char* p_preset,
						 const Vec2i& position,
						 uint index)
	{
		const float f = level.getHudIconFlash(index);
		if(f <= 0.0f) return;

		Engine& engine = Engine::inst();
		Renderer::inst().setBlend(BM_ADDITIVE);
		level.getPresets()->renderPreset(p_preset, position, Vec4f(f, f, f, 1.0f));
		Renderer::inst().setBlend(BM_NORMAL);
	}
}
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
		bool onPlayer = false;
		Vec2i c = game.engine.getCursorPosition() / 16;
		std::vector<Object*> objects = game.p_level->getObjectsAt(c);
		for(std::vector<Object*>::const_iterator i = objects.begin(); i != objects.end(); ++i)
		{
			Object* p_obj = *i;
			if(p_obj->getType() == "Player")
			{
				static_cast<Player*>(p_obj)->activate();
				onPlayer = true;
			}
		}

		// The press that wakes a character also takes hold of it for a drag,
		// and this is the one place that knows a press landed on one. A press
		// elsewhere works the cell - unless a character is already held, where
		// a second button means a bomb and lands wherever the cursor has got
		// to. Hence the local for this press and the flag for the grip.
		if(onPlayer) game.dragFromPlayer = true;
		else if(!game.dragFromPlayer) game.bumpCell(c);
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
			// "MenuPane.Menu" is hidden and the key belongs to the dialog. And
			// it is Continue, so not while that is greyed out because nobody
			// is left to play.
			if(pressed && event.keysym.sym == SDLK_ESCAPE &&
			   getChild("MenuPane.Menu")->isReallyVisible() &&
			   getChild("MenuPane.Menu.Continue")->isActive())
			{
				handleClick(getChild("MenuPane.Menu.Continue"));
			}
			return;
		}

		if(!pressed) return;

		// An open hint note takes Return and Escape first: it covers the play
		// area, and the player would otherwise have to walk off the field.
		// With nothing to close, dismissDisplay() is false and Escape goes on
		// to the game menu.
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
			// Opening the menu drops a drag, or a button still held while the
			// player reads it would walk the character. Escape and the button
			// on screen both come through here.
			game.engine.cancelMouseDrag();

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
			if(game.cameFromEditor) game.engine.crossfade(new CF_Mosaic, 0.85f);
			else game.engine.crossfade(new CF_Cube, 0.85f);
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

GS_Game::GS_Game() : GameState("GS_Game"), engine(Engine::inst()), levelNumber(0), p_currentCampaign(0), showCursor(0), ignoreNextCursorMovement(false), dragFromPlayer(false)
{
	// Zeroed here and not only in onEnter(): a pushed state is on the stack,
	// and getGameState() names it, before processGameStateChanges() runs
	// onEnter() - and the test hooks ask in that window.
	p_level = 0;
	p_selectLevel = 0;
	p_misc = 0;
	p_originalLevel = 0;
	p_saveGame = 0;
}

GS_Game::~GS_Game()
{
}

void GS_Game::onRender()
{
	// render the level
	p_level->render();

	// render the panel: the strip below the level's picture in the
	// background image
	Renderer& renderer = Renderer::inst();
	const Vec2f corners[4] = {Vec2f(0.0f, 400.0f), Vec2f(640.0f, 400.0f), Vec2f(640.0f, 480.0f), Vec2f(0.0f, 480.0f)};
	const Vec2f uvs[4] = {Vec2f(0.0f, 480.0f), Vec2f(640.0f, 480.0f), Vec2f(640.0f, 560.0f), Vec2f(0.0f, 560.0f)};
	renderer.setTexture(p_level->getBackground()->ref());
	renderer.quad(renderer.state(), corners, uvs, Vec4f(1.0f, 1.0f, 1.0f, 1.0f));

	Font* p_font = gui.getFont();

	// render the data
	char text[256] = "";
	uint nd = p_level->getNumDiamondsCollected();
	p_level->getPresets()->renderPreset("Diamond", Vec2i(32, 416));
	renderIconFlash(*p_level, "Diamond", Vec2i(32, 416), 1);
	sprintf(text, "%d/%d", nd, p_level->getNumDiamondsNeeded());
	float alpha;
	if(nd >= p_level->getNumDiamondsNeeded()) alpha = 0.7f + 0.3f * sinf(static_cast<float>(p_level->counter) * 0.4f);
	else alpha = 1.0f;
	p_font->renderText(text, Vec2i(66, 416), Vec4f(1.0f, 1.0f, 1.0f, alpha));

	Player* p_player = p_level->getActivePlayer();
	if(p_player)
	{
		p_level->getPresets()->renderPreset("Bomb", Vec2i(32, 448));
		renderIconFlash(*p_level, "Bomb", Vec2i(32, 448), 0);
		sprintf(text, "%d", p_player->getInventory(0));
		p_font->renderText(text, Vec2i(66, 448), Vec4f(1.0f, 1.0f, 1.0f, 1.0f));

		float c = static_cast<float>(p_player->getContamination());
		if(c > 0.0f)
		{
			Engine& engine = Engine::inst();
			float t = static_cast<float>(engine.getTime()) / 1000.0f;
			float x = min(0.5f, 0.002f * c);
			float f = c < 375.0f ? 2.0f : 4.0f;
			float s = sinf(6.282f * f * t);
			Vec4f color(1.0f, 1.0f - x, 1.0f - x, 0.7f + 0.4f * s);
			float scaling = 1.0f + min(0.2f, 0.002f * c) * s;
			engine.renderSprite(p_misc, Vec2i(168, 416), Vec2i(0, 0), Vec2i(48, 48), color, false, 0.0f, scaling);
		}
	}

	// The level number belongs to the campaign. A trial run from the editor and
	// a single level have none; the single level names its file instead, to
	// keep three levels of the same name apart.
	const std::string title = localizeString(p_level->getTitle());

	// The space between the image at 168 (48 wide) and the menu button at
	// 544, centred on 384: the button is the tighter side, hence
	// 2 * (544 - 8 - 384), eight pixels of gap. renderText() clips nothing, so
	// the rest has to go.
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
	p_font->renderText(caption, Vec2i(384 - dim.x / 2, 432), Vec4f(1.0f, 1.0f, 1.0f, 1.0f));

	if(paused)
	{
		float t = 0.001f * engine.getTime();
		float r = 0.5f + 0.5f * sinf(3.0f * t);
		float g = 0.5f + 0.5f * cosf(4.27f * t);
		float b = 0.5f + 0.5f * sinf(5.13f * t);
		float a = 0.7f + 0.3f * sinf(6.26f * t);
		p_font->renderText("Pause", pausePosition, Vec4f(r, g, b, a));
	}
}

bool GS_Game::getMouseDragCells(Vec2i* p_actor,
								Vec2i* p_target)
{
	if(!p_level || paused || p_level->isInPreview()) return false;
	if(GUI::inst()["Game.MenuPane"]->isVisible()) return false;

	// Only a press that landed on the field, which the GUI remembers as the
	// GameGUI itself; a press on a widget - the Menu button, a pad key - must
	// not walk the character.
	GUI_Element* p_down = GUI::inst().getMouseDownElement();
	if(!p_down || p_down->getFullName() != "Game") return false;

	// And it has to have landed on a character.
	if(!dragFromPlayer) return false;

	Player* p_player = p_level->getActivePlayer();
	if(!p_player) return false;

	*p_actor = p_player->getPosition();

	// The same arithmetic GameGUI::onMouseDown uses to find a character under
	// the cursor. The level's own offset is camera shake and nothing else -
	// the field is never scrolled - so there is none to take off here.
	*p_target = Engine::inst().getCursorPosition() / 16;
	return true;
}

bool GS_Game::canMouseDragStep(const Vec2i& dir)
{
	if(!p_level) return false;
	Player* p_player = p_level->getActivePlayer();
	return p_player ? p_player->move(dir, false, true) : false;
}

// Clicking on something the character stands next to works it: a switch or a
// magnet does its whole job in onTouchedByPlayer, reached by walking into it,
// and the drag never walks into what it cannot enter. Two guards: orthogonally
// adjacent, and only where the character cannot go, so a click is never a
// step or a push (a panel is walked onto and stays a place to stand). The rest
// is move()'s, so a click reaches exactly what a walk that way would - which
// for a switch standing on a solid tile is nothing.
void GS_Game::bumpCell(const Vec2i& cell)
{
	if(!p_level || paused || p_level->isInPreview()) return;
	if(GUI::inst()["Game.MenuPane"]->isVisible()) return;

	Player* p_player = p_level->getActivePlayer();
	if(!p_player) return;

	const Vec2i dir = cell - p_player->getPosition();
	if(abs(dir.x) + abs(dir.y) != 1) return;
	if(p_player->move(dir, false, true)) return;

	p_player->move(dir);
}

void GS_Game::onUpdate()
{

	// The hold on a character lasts until the last button is up, read off the
	// buttons' state: losing the focus clears them without a release event.
	if(!engine.isButtonDown(SDL_BUTTON_LEFT) && !engine.isButtonDown(SDL_BUTTON_RIGHT))
	{
		dragFromPlayer = false;
	}

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

	// Any key or click leaves the pause, so the click back from another
	// window (which pauses) resumes. The press is spent on that, or the pause
	// key would switch straight back on what it has just switched off.
	if(paused && (engine.wasAnyKeyPressed() || engine.wasAnyButtonPressed()))
	{
		paused = false;
	}
	else if(!menuVisible)
	{
		if(engine.wasActionPressed("$A_SWITCH_CHARACTER"))
		{
			// select the next player
			p_level->switchToNextPlayer();
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
		// Greater than zero, not non-zero: below zero means syringes in
		// reserve, better than clean - nothing crackles and no gas spreads.
		int c = p_player->getContamination();
		if(c > 0)
		{
			if(c >= 50)
			{
				p_level->addToxic(0.05f);
			}

			if(random(0, 2000 + c) >= 2000)
			{
				// play the Geiger counter sound
				Engine::inst().playSound("geiger.ogg", false, 0.2f);
			}
		}
	}

	// level completed?
	if(p_level->finished)
	{
		// A single level belongs to no campaign: nothing to record, and the
		// next entry is not the next step. So, as after a trial run from the
		// editor, back to where the player came from.
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
		else status = loadLevel(true);
		if(status == 1)
		{
			// Nothing makes a level have an active character, and the zoom
			// then ends in the middle.
			Player* p_player = p_level->getActivePlayer();
			targetOut = p_player ? p_player->getShownPositionInPixels() + Vec2i(8, 8) : Vec2i(320, 240);
			Engine::inst().crossfade(new CF_Zoom(targetIn, targetOut), 3.0f);
		}
		else
		{
			targetOut = Vec2i(320, 240);

			if(status == -1)
			{
				// The final level is done. Only the shipped campaign ends in
				// the credits, every other returns to the selection; asked by
				// filename, which is what Transfer::isBuiltIn() takes.
				if(Transfer::isBuiltIn(Transfer::KIND_CAMPAIGN,
									   FileSystem::inst().getPathFilename(p_currentCampaign->getFilename())))
				{
					// The game is over.
					Engine::inst().setGameState("GS_Credits");
					Engine::inst().crossfade(new CF_ColorBlend(Vec3f(0.0f, 0.0f, 0.0f), 0.5f), 2.0f);
				}
				else
				{
					Engine::inst().popGameState();
					Engine::inst().crossfade(new CF_ColorBlend(Vec3f(0.0f, 0.0f, 0.0f), 0.5f), 2.0f);
					if(p_selectLevel) p_selectLevel->setCurrentLevel(levelNumber - 1);
				}
			}
			else if(status == -2)
			{
				// The next level is the bonus level.
				Engine::inst().popGameState();
				Engine::inst().crossfade(new CF_Zoom(targetIn, targetOut), 3.0f);
				if(p_selectLevel) p_selectLevel->setCurrentLevel(levelNumber);
			}
			else if(status == 0)
			{
				// error
				Engine::inst().crossfade(new CF_Zoom(targetIn, targetOut), 3.0f);
				if(p_selectLevel) p_selectLevel->setCurrentLevel(levelNumber - 1);
			}
			else if(status == -3)
			{
				Engine::inst().popGameState();
				Engine::inst().crossfade(new CF_Zoom(targetIn, targetOut), 3.0f);
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
		pausePosition += 0.02f * 190.0f * pauseVelocity;

		if(pausePosition.x < 0.0f || pausePosition.x > 600.0f)
		{
			pauseVelocity.x *= -1.0f;
			pauseVelocity.y += random(-0.2f, 0.2f);
			pauseVelocity.normalize();
		}

		if(pausePosition.y < 0.0f || pausePosition.y > 382.0f)
		{
			pauseVelocity.y *= -1.0f;
			pauseVelocity.x += random(-0.2f, 0.2f);
			pauseVelocity.normalize();
		}

		pausePosition += 0.02f * 10.0f * pauseVelocity;
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
	pausePosition = Vec2f(320.0f, 200.0f);
	const float r = random(0.0f, 6.2832f);
	pauseVelocity = Vec2f(sinf(r), cosf(r));

	// load the level
	p_level = new Level;
	if(cameFromEditor)
	{
		TiXmlDocument* p_doc = context.get<TiXmlDocument*>("levelDocument");
		p_level->load(p_doc);
		delete p_doc;

		// save the level
		p_originalLevel = p_level->save();

		// Play the music: a file beside the level, or a shipped campaign track
		// with "blocks:". A trial run from the editor is a document with no
		// path, so "beside it" means the player's level folder.
		Engine::inst().playMusic(Campaign::resolveMusicPath(p_level->getMusicFilename(),
														   FileSystem::inst().getAppHomeDirectory() + "levels/"));
	}
	else
	{
		levelNumber = context.get<uint>("levelNumber");
		p_currentCampaign = context.get<Campaign*>("campaign");
		loadLevel(false);
	}

	// load the images
	p_misc = Manager<Texture>::inst().request("misc.png");

	leaveCountDown = 50;

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

	// Forget whatever was held when the focus went: onUpdate, which notices
	// the release, did not run meanwhile.
	dragFromPlayer = false;
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

int GS_Game::loadLevel(bool checkBonus)
{
	delete p_saveGame;
	p_saveGame = 0;

	// Is this the last level, and does the campaign have a bonus level? Asked
	// only on the way on from a finished level: a level picked in the
	// selection is one the selection offered, and it offers a bonus level
	// once solved - which a merged progress can say of the bonus level
	// alone - so refusing it here would leave nothing loaded.
	if(checkBonus &&
	   levelNumber == p_currentCampaign->getLevels().size() - 1 &&
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

	// What a restart goes back to, the error level included where the file
	// would not load: kept from before, it would be the previous level
	// restarted under this one's number, or no level at all.
	delete p_originalLevel;
	p_originalLevel = p_level->save();

	delete p_oldLevel;

	// Play the music - from the campaign's archive, or from blocks.zip if the
	// level takes the track from there with "blocks:".
	Engine::inst().playMusic(Campaign::resolveMusicPath(p_level->getMusicFilename(), ref.sourceDir));

	return r ? 1 : 0;
}