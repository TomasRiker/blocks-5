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
#include "streamedsound.h"
#include "texture.h"
#include "options.h"
#include "help.h"
#include "campaign.h"
#include "progressdb.h"
#include "hotel.h"

const std::string pw = "[3Cs18Ab0bV0Aat3Wf27le1ZM12kt0Xs05Aa4PX1EyI2V112Jr26v2GZO3dN0Ec91hk024P3cA32bc3GZ07Em4bf34st4320F7d13S00wd4Mg1ANn4SF2EO94Hz13Qq0LO18iY4Qy2C8r2XF28Bh]";

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

		// Wurde eine Spielfigur angeklickt?
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
		bool menuVisible = GUI::inst()["Game.MenuPane"]->isVisible();
		if(menuVisible) return;

		if(event.type == SDL_KEYUP && event.keysym.sym == SDLK_TAB) game.switchTimer = 0;

		// Uns interessiert nur, ob eine Taste gedrückt wurde.
		if(event.type != SDL_KEYDOWN) return;

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
			// Level wiederherstellen
			Level* p_oldLevel = game.p_level;
			p_oldLevel->clean();
			p_oldLevel->removeOldObjects();
			game.p_level = new Level;
			game.p_level->load(game.p_originalLevel);
			delete p_oldLevel;
			game.engine.crossfade(new CF_Slices, 0.85);
			getChild("MenuPane")->hide();
			focus();
			game.leaveCountDown = 50;

			// TODO: Bug fixen: Wenn man runterfällt und im richtigen Moment F5 drückt (etwas länger warten), ist Player::numInstances an dieser Stelle 0, obwohl der Level neu gestartet wurde!
		}
		else if(name == "Game.MenuPane.Menu.RestartFromHotel")
		{
			// Level wiederherstellen
			Level* p_oldLevel = game.p_level;
			p_oldLevel->clean();
			p_oldLevel->removeOldObjects();
			game.p_level = new Level;
			game.p_level->load(game.p_saveGame);
			delete p_oldLevel;
			game.engine.crossfade(new CF_Slices, 0.85);
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
	// Level rendern
	p_level->render();

	// Panel rendern
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

	// Daten rendern
	char text[256] = "";
	uint nd = p_level->getNumDiamondsCollected();
	p_level->getPresets()->renderPreset("Diamond", Vec2i(32, 416));
	sprintf(text, "%d/%d", nd, p_level->getNumDiamondsNeeded());
	double alpha;
	if(nd >= p_level->getNumDiamondsNeeded()) alpha = 0.7 + 0.3 * sin(static_cast<double>(p_level->counter) * 0.4);
	else alpha = 1.0;
	p_font->renderText(text, Vec2i(66, 416), Vec4d(1.0, 1.0, 1.0, alpha));

	Player* p_player = p_level->getActivePlayer();
	if(p_player)
	{
		p_level->getPresets()->renderPreset("Bomb", Vec2i(32, 448));
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

	std::string title = localizeString(p_level->getTitle());

	if(cameFromEditor) sprintf(text, "%s", title.c_str());
	else sprintf(text, "%02d - %s", levelNumber + 1, title.c_str());
	Vec2i dim;
	p_font->measureText(text, &dim, 0);
	p_font->renderText(text, Vec2i(384 - dim.x / 2, 432), Vec4d(1.0, 1.0, 1.0, 1.0));

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

	if(!menuVisible)
	{	
		if(engine.wasActionPressed("$A_SWITCH_CHARACTER"))
		{
			if(!switchTimer)
			{
				// nächste Spielfigur auswählen
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

	// Spieler tot?
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

	// Spieler vergiftet?
	Player* p_player = p_level->getActivePlayer();
	if(p_player)
	{
		int c = p_player->getContamination();
		if(c)
		{
			if(c >= 50)
			{
				p_level->addToxic(0.05);
			}

			if(random(0, 2000 + c) >= 2000)
			{
				// Geigerzähler-Geräusch abspielen
				Engine::inst().playSound("geiger.ogg", false, 0.2);
			}
		}
	}

	// Level geschafft?
	if(p_level->finished)
	{
		if(!cameFromEditor)
		{
			// Fortschritt vermerken
			ProgressDB& db = ProgressDB::inst();
			bool old = db.wasLevelCompleted(p_currentCampaign->getFilename(), levelNumber);
			db.setLevelCompleted(p_currentCampaign->getFilename(), levelNumber);
			db.save();
		}

		// nächster Level oder zurück zum Menü
		Vec2i targetIn = p_level->getExit()->getShownPositionInPixels() + Vec2i(8, 8);
		Vec2i targetOut;
		levelNumber++;
		int status;
		if(cameFromEditor) status = -3;
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
				// Das war der letzte Level.
				if(p_currentCampaign->getFilename() == FileSystem::inst().getAppHomeDirectory() + "levels/campaigns/blocks.zip")
				{
					// Das Spiel ist vorbei!
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
				// Der nächste Level ist der Bonus-Level.
				Engine::inst().popGameState();
				Engine::inst().crossfade(new CF_Zoom(targetIn, targetOut), 3.0);
				if(p_selectLevel) p_selectLevel->setCurrentLevel(levelNumber);
			}
			else if(status == 0)
			{
				// Fehler
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
		// Level bewegen
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
	paused = false;
	pausePosition = Vec2d(320.0, 200.0);
	const double r = random(0.0, 6.2832);
	pauseVelocity = Vec2d(sin(r), cos(r));

	// Level laden
	p_level = new Level;
	if(cameFromEditor)
	{
		TiXmlDocument* p_doc = context.get<TiXmlDocument*>("levelDocument");
		p_level->load(p_doc);
		delete p_doc;

		// Level speichern
		p_originalLevel = p_level->save();

		// Musik abspielen
		std::string music = p_level->getMusicFilename();
		Engine::inst().playMusic(music.empty() ? "" : FileSystem::inst().getAppHomeDirectory() + "levels/" + music);
	}
	else
	{
		levelNumber = context.get<uint>("levelNumber");
		p_currentCampaign = context.get<Campaign*>("campaign");
		loadLevel();
	}

	// Bilder laden
	p_misc = Manager<Texture>::inst().request("misc.png");

	leaveCountDown = 50;
	switchTimer = 0;

	// Dialog erzeugen
	new GameGUI(*this);
}

void GS_Game::onLeave(const ParameterBlock& context)
{
	// Ressourcen löschen
	delete p_level;
	delete p_originalLevel;
	delete p_saveGame;
	p_misc->release();
	p_level = 0;
	p_originalLevel = 0;
	p_saveGame = 0;
	p_misc = 0;

	// Dialog löschen
	delete gui["Game"];

	// Musik stoppen
	Engine::inst().stopMusic();

	// dem Levelauswahl-Bildschirm mitteilen, in welchem Level wir gerade sind
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

	// Ist dies der letzte Level, und hat die Kampagne einen Bonus-Level?
	if(levelNumber == p_currentCampaign->getLevels().size() - 1 &&
	   p_currentCampaign->hasBonusLevel())
	{
		// Wurden weniger Levels geschafft als notwendig?
		if(ProgressDB::inst().getNumLevelsCompleted(p_currentCampaign->getFilename()) < p_currentCampaign->getLevels().size() - 1)
		{
			return -2;
		}
	}

	if(levelNumber >= p_currentCampaign->getLevels().size())
	{
		// Dies war der letzte Level der Kampagne.
		return -1;
	}

	char levelName[256];
	sprintf(levelName, "level_%d.xml", levelNumber + 1);

	Level* p_oldLevel = p_level;
	p_level = new Level;
	bool r = p_level->load(p_currentCampaign->getFilename() + pw + "/" + levelName);
	if(r)
	{
		// Level speichern
		delete p_originalLevel;
		p_originalLevel = p_level->save();
	}

	delete p_oldLevel;

	// Musik abspielen
	Engine::inst().playMusic(p_level->getMusicFilename().empty() ? "" : p_currentCampaign->getFilename() + pw + "/" + p_level->getMusicFilename());

	return r ? 1 : 0;
}