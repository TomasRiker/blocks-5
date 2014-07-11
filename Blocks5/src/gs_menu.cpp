#include "pch.h"
#include "gs_menu.h"
#include "gui.h"
#include "gui_all.h"
#include "cf_all.h"
#include "texture.h"
#include "options.h"
#include "help.h"
#include "filesystem.h"
#include <shellapi.h>

extern const char* p_localVersion;

GS_Menu::GS_Menu() : GameState("GS_Menu"), engine(Engine::inst()), titleLevelXML("")
{
	p_clouds = 0;
	p_background = 0;
	p_titleLevel = 0;
	levelSaved = false;
}

GS_Menu::~GS_Menu()
{
}

void GS_Menu::onRender()
{
	// Farbübergang rendern
	glBegin(GL_QUADS);
	glColor3d(0.5, 0.5, 1.0);
	glVertex2i(0, 0);
	glVertex2i(640, 0);
	glColor3d(0.75, 0.7, 1.0);
	glVertex2i(640, 480);
	glVertex2i(0, 480);
	glEnd();

	// Wolken rendern
	p_clouds->bind();
	glMatrixMode(GL_TEXTURE);

	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_NOTEQUAL, 0.0f);

	for(int i = 2; i >= 0; i--)
	{
		double s[] = {1.0, 0.5, 0.25};
		double x = 100.0 * i + 50.0 * 0.001 * time;
		x += 2.0 * sin(0.02 * x * s[i] + i);

		glPushMatrix();
		glScaled(s[i], s[i], s[i]);
		glTranslated(-x / s[i], 0.0, 0.0);
		glRotated(15.0 + 5.0 * i, 0.0, 0.0, 1.0);
		glBegin(GL_QUADS);
		const double c = 1.0 - 0.05 * i;
		const double a = 0.4 - 0.05 * i;
		glColor4d(c, c, c, a);
		glTexCoord2i(0, 0);
		glVertex2i(0, 0);
		glTexCoord2i(640, 0);
		glVertex2i(640, 0);
		glTexCoord2i(640, 480);
		glVertex2i(640, 480);
		glTexCoord2i(0, 480);
		glVertex2i(0, 480);
		glEnd();
		glPopMatrix();
	}

	glDisable(GL_ALPHA_TEST);

	p_clouds->unbind();
	glMatrixMode(GL_MODELVIEW);

	// Titel-Level rendern
	glPushMatrix();
	glTranslated(0.0, 65.0, 0.0);
	p_titleLevel->render();
	glPopMatrix();

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
}

void GS_Menu::onUpdate()
{
	Uint8* p_keyStates = SDL_GetKeyState(0);
	if(p_keyStates[SDLK_c] &&
	   (p_keyStates[SDLK_LSHIFT] ||
	    p_keyStates[SDLK_RSHIFT]))
	{
		engine.setGameState("GS_Credits");
	}
	else if(p_keyStates[SDLK_d] &&
		(p_keyStates[SDLK_LSHIFT] ||
		p_keyStates[SDLK_RSHIFT]))
	{
		FileSystem& fs = FileSystem::inst();
		fs.writeStringToFile("disable", fs.getAppHomeDirectory() + ".donation_asked");
	}

	for(int i = 0; i < 512; i++)
	{
		engine.setKeyData(static_cast<SDLKey>(i), 0);
	}

	stdext::hash_map<uint, std::list<uint> >::const_iterator i = keyData.find(time - 500);
	if(i != keyData.end())
	{
		const std::list<uint>& list = i->second;
		std::list<uint>::const_iterator j = list.begin();
		while(j != list.end())
		{
			uint key = *j; j++;
			int data = *j; j++;
			engine.setKeyData(static_cast<SDLKey>(key), data);
		}
	}

	if(engine.wasKeyPressed(SDLK_TAB))
	{
		p_titleLevel->switchToNextPlayer();
	}

	p_titleLevel->update();

	time += 20;
}

void GS_Menu::onEnter(const ParameterBlock& context)
{
	// Bilder laden
	p_clouds = Manager<Texture>::inst().request("clouds.png");
	p_background = Manager<Texture>::inst().request("menu.png");

	// Menü erzeugen
	gui.getRoot()->load("menu.xml");

	static_cast<GUI_StaticText*>(gui["Menu.Version"])->setText(p_localVersion);
	static_cast<GUI_Button*>(gui["Menu.StartGame"])->connectClicked(this, &GS_Menu::handleClick);
	static_cast<GUI_Button*>(gui["Menu.LevelEditor"])->connectClicked(this, &GS_Menu::handleClick);
	static_cast<GUI_Button*>(gui["Menu.CampaignEditor"])->connectClicked(this, &GS_Menu::handleClick);
	static_cast<GUI_Button*>(gui["Menu.Options"])->connectClicked(this, &GS_Menu::handleClick);
	static_cast<GUI_Button*>(gui["Menu.Help"])->connectClicked(this, &GS_Menu::handleClick);
	static_cast<GUI_Button*>(gui["Menu.Quit"])->connectClicked(this, &GS_Menu::handleClick);
	static_cast<GUI_Button*>(gui["Menu.Donate"])->connectClicked(this, &GS_Menu::handleClick);
	static_cast<GUI_Button*>(gui["Menu.DonatePane.Donate.NoThanks"])->connectClicked(this, &GS_Menu::handleClick);
	static_cast<GUI_Button*>(gui["Menu.DonatePane.Donate.Donate"])->connectClicked(this, &GS_Menu::handleClick);

	// Wann wurde zuletzt nach einer Spende gefragt?
	FileSystem& fs = FileSystem::inst();
	const std::string lastAskedForDonationStr = fs.readStringFromFile(fs.getAppHomeDirectory() + ".donation_asked");
	if(lastAskedForDonationStr != "disable")
	{
		const uint lastAskedForDonation = static_cast<uint>(atoi(lastAskedForDonationStr.c_str()));
		if(engine.getTimePlayed() - lastAskedForDonation >= 60 * (60 * 60 * 3)) gui["Menu.DonatePane.Donate"]->focus();
	}

	p_options = new Options(0);
	p_help = new Help(0);

	// Keyboard-Daten für die Demo laden
	keyData.clear();
	File* p_file = FileSystem::inst().openFile("demo1.dat", FileSystem::FM_READ);
	while(!p_file->isEOF())
	{
		uint t;
		p_file->read(&t, 4);
		while(true)
		{
			uint key;
			int data;
			p_file->read(&key, 4);
			if(key == ~0) break;
			p_file->read(&data, 4);
			keyData[t].push_back(key);
			keyData[t].push_back(data);
		}
	}
	FileSystem::inst().closeFile(p_file);

	SDL_ShowCursor(1);
}

void GS_Menu::onLeave(const ParameterBlock& context)
{
	// Bilder löschen
	p_clouds->release();
	p_clouds = 0;
	p_background->release();
	p_background = 0;

	if(p_titleLevel) delete p_titleLevel;
	p_titleLevel = 0;
	levelSaved = false;

	// Menü löschen
	delete gui["Menu"];
	delete p_options;
	delete p_help;
}

void GS_Menu::onGetFocus()
{
	engine.playMusic("menu.ogg");

	// Titel-Level laden
	p_titleLevel = new Level;
	p_titleLevel->setInMenu(true);
	if(levelSaved) p_titleLevel->load(&titleLevelXML);
	else
	{
		p_titleLevel->load("title.xml");
		time = 0;
	}

	gui["Menu"]->focus();
}

void GS_Menu::onLoseFocus()
{
	// Titel-Level speichern und löschen
	TiXmlDocument* p_doc = p_titleLevel->save();
	titleLevelXML = *p_doc;
	delete p_doc;
	delete p_titleLevel;
	p_titleLevel = 0;
	levelSaved = true;

	gui["Menu"]->hide();
}

void GS_Menu::handleClick(GUI_Element* p_element)
{
	const std::string& name = p_element->getFullName();
	if(name == "Menu.StartGame")
	{
		engine.pushGameState("GS_SelectLevel");
		engine.crossfade(new CF_Star, 0.85);
	}
	else if(name == "Menu.LevelEditor")
	{
		engine.pushGameState("GS_LevelEditor");
		engine.crossfade(new CF_Star, 0.85);
	}
	else if(name == "Menu.CampaignEditor")
	{
		engine.pushGameState("GS_CampaignEditor");
		engine.crossfade(new CF_Star, 0.85);
	}
	else if(name == "Menu.Options")
	{
		p_options->show(gui["Menu"]);
	}
	else if(name == "Menu.Help")
	{
		p_help->show(gui["Menu"]);
	}
	else if(name == "Menu.Quit")
	{
		SDL_Event event;
		event.type = SDL_QUIT;
		SDL_PushEvent(&event);
	}
	else if(name == "Menu.Donate")
	{
		gui["Menu.DonatePane.Donate"]->focus();
	}
	else if(name == "Menu.DonatePane.Donate.NoThanks" ||
			name == "Menu.DonatePane.Donate.Donate")
	{
		gui["Menu.DonatePane"]->hide();
		gui["Menu"]->focus();

		FileSystem& fs = FileSystem::inst();
		if(fs.readStringFromFile(fs.getAppHomeDirectory() + ".donation_asked") != "disable")
		{
			// merken, dass wir nach einer Spende gefragt haben
			std::ostringstream temp;
			temp << engine.getTimePlayed();
			fs.writeStringToFile(temp.str(), fs.getAppHomeDirectory() + ".donation_asked");
		}

		if(name == "Menu.DonatePane.Donate.Donate")
		{
			gui["Menu.DonatePane"]->hide();
			std::string urlPath(std::string("Donate (") + engine.getLanguage() + ").url");
			ShellExecuteA(0, "open", urlPath.c_str(), 0, 0, SW_SHOWMAXIMIZED);
		}
	}
}