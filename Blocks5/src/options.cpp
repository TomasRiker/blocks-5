#include "pch.h"
#include "options.h"
#include "engine.h"
#include "gui_all.h"

Options::Options(GUI_Element* p_parent) : GUI_Element("OptionsPane", p_parent, Vec2i(0, 0), Vec2i(640, 480))
{
	load("options.xml");
	hide();

	static_cast<GUI_Button*>(getChild("Options.OK"))->connectClicked(this, &Options::handleClick);
	static_cast<GUI_Button*>(getChild("Options.Cancel"))->connectClicked(this, &Options::handleClick);
	static_cast<GUI_RadioButton*>(getChild("Options.English"))->connectChanged(this, &Options::handleClick);
	static_cast<GUI_RadioButton*>(getChild("Options.German"))->connectChanged(this, &Options::handleClick);
	static_cast<GUI_ScrollBar*>(getChild("Options.SoundVolume"))->connectChanged(this, &Options::handleClick);
	static_cast<GUI_ScrollBar*>(getChild("Options.MusicVolume"))->connectChanged(this, &Options::handleClick);
	static_cast<GUI_RadioButton*>(getChild("Options.LowDetails"))->connectChanged(this, &Options::handleClick);
	static_cast<GUI_RadioButton*>(getChild("Options.MediumDetails"))->connectChanged(this, &Options::handleClick);
	static_cast<GUI_RadioButton*>(getChild("Options.HighDetails"))->connectChanged(this, &Options::handleClick);
	static_cast<GUI_ListBox*>(getChild("Options.Actions"))->connectChanged(this, &Options::handleClick);
	static_cast<GUI_Button*>(getChild("Options.ResetControls"))->connectClicked(this, &Options::handleClick);
	static_cast<GUI_Button*>(getChild("Options.PrimaryKey"))->connectClicked(this, &Options::handleClick);
	static_cast<GUI_Button*>(getChild("Options.SecondaryKey"))->connectClicked(this, &Options::handleClick);

	// Aktionen eintragen
	GUI_ListBox* p_actions = static_cast<GUI_ListBox*>(getChild("Options.Actions"));
	const std::vector<Action*>& actions = Engine::inst().getActionsVector();
	for(std::vector<Action*>::const_iterator it = actions.begin();
		it != actions.end();
		++it)
	{
		p_actions->addItem(GUI_ListBox::ListItem((*it)->name.c_str()));
	}

	p_focusWhenClosed = 0;
	changed = false;
}

Options::~Options()
{
}

void Options::show(GUI_Element* p_focusWhenClosed)
{
	this->p_focusWhenClosed = p_focusWhenClosed;

	Engine& engine = Engine::inst();

	// aktuelle Sprache setzen
	if(engine.getLanguage() == "en") static_cast<GUI_RadioButton*>(getChild("Options.English"))->check();
	else if(engine.getLanguage() == "de") static_cast<GUI_RadioButton*>(getChild("Options.German"))->check();

	// aktuelle Sound-Lautstärke setzen
	static_cast<GUI_ScrollBar*>(getChild("Options.SoundVolume"))->setScroll(static_cast<int>(100.0 * engine.getSoundVolume()));

	// aktuelle Musik-Lautstärke setzen
	static_cast<GUI_ScrollBar*>(getChild("Options.MusicVolume"))->setScroll(static_cast<int>(100.0 * engine.getMusicVolume()));

	// aktuelle Details setzen
	if(engine.getDetails() == 0) static_cast<GUI_RadioButton*>(getChild("Options.LowDetails"))->check();
	else if(engine.getDetails() == 1) static_cast<GUI_RadioButton*>(getChild("Options.MediumDetails"))->check();
	else if(engine.getDetails() == 2) static_cast<GUI_RadioButton*>(getChild("Options.HighDetails"))->check();

	static_cast<GUI_ListBox*>(getChild("Options.Actions"))->setSelection(-1);
	static_cast<GUI_Button*>(getChild("Options.PrimaryKey"))->setTitle("");
	static_cast<GUI_Button*>(getChild("Options.SecondaryKey"))->setTitle("");

	getChild("Options")->focus();
}

void Options::handleClick(GUI_Element* p_element)
{
	std::string name = p_element->getName();
	Engine& engine = Engine::inst();

	if(isVisible())
	{
		// Sprache speichern
		if(static_cast<GUI_RadioButton*>(getChild("Options.German"))->isChecked()) engine.setLanguage("de");
		else if(static_cast<GUI_RadioButton*>(getChild("Options.English"))->isChecked()) engine.setLanguage("en");

		// Sound-Lautstärke speichern
		engine.setSoundVolume((1.0 / 100.0) * static_cast<GUI_ScrollBar*>(getChild("Options.SoundVolume"))->getScroll());

		// Musik-Lautstärke speichern
		engine.setMusicVolume((1.0 / 100.0) * static_cast<GUI_ScrollBar*>(getChild("Options.MusicVolume"))->getScroll());

		// Details speichern
		if(static_cast<GUI_RadioButton*>(getChild("Options.LowDetails"))->isChecked()) engine.setDetails(0);
		else if(static_cast<GUI_RadioButton*>(getChild("Options.MediumDetails"))->isChecked()) engine.setDetails(1);
		else if(static_cast<GUI_RadioButton*>(getChild("Options.HighDetails"))->isChecked()) engine.setDetails(2);

		if(name == "Actions")
		{
			GUI_Button* p_primary = static_cast<GUI_Button*>(getChild("Options.PrimaryKey"));
			GUI_Button* p_secondary = static_cast<GUI_Button*>(getChild("Options.SecondaryKey"));
			GUI_ListBox* p_actions = static_cast<GUI_ListBox*>(getChild("Options.Actions"));
			const std::vector<Action*>& actions = Engine::inst().getActionsVector();

			int selection = p_actions->getSelection();
			if(selection == -1)
			{
				p_primary->setTitle("");
				p_secondary->setTitle("");
			}
			else
			{
				const Action& action = *(actions[selection]);
				const std::vector<VirtualKey>& vks = Engine::inst().getVKs();
				p_primary->setTitle(action.primary == -1 ? "$O_NOT_ASSIGNED" : vks[action.primary].name);
				p_secondary->setTitle(action.secondary == -1 ? "$O_NOT_ASSIGNED" : vks[action.secondary].name);
			}
		}
		else if(name == "PrimaryKey" || name == "SecondaryKey")
		{
			GUI_ListBox* p_actions = static_cast<GUI_ListBox*>(getChild("Options.Actions"));
			int selection = p_actions->getSelection();
			if(selection != -1)
			{
				const std::vector<Action*>& actions = Engine::inst().getActionsVector();
				const Action& action = *(actions[selection]);

				int key = Engine::inst().getPressedVK(3000);
				if(name == "PrimaryKey") Engine::inst().changeAction(action.name, key, action.secondary);
				else Engine::inst().changeAction(action.name, action.primary, key);

				SDL_Delay(250);
				Engine::inst().updateVKs();

				handleClick(p_actions);
			}
		}
		else if(name == "ResetControls")
		{
			Engine::inst().resetActions();
			GUI_ListBox* p_actions = static_cast<GUI_ListBox*>(getChild("Options.Actions"));
			handleClick(p_actions);
		}
		else if(name == "OK")
		{
			engine.saveConfig();

			hide();
			if(p_focusWhenClosed) p_focusWhenClosed->focus();
		}
		else if(name == "Cancel")
		{
			if(changed) engine.loadConfig();

			hide();
			if(p_focusWhenClosed) p_focusWhenClosed->focus();
		}

		changed = true;
	}
}