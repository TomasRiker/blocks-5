#include "pch.h"
#include "options.h"
#include "engine.h"
#include "gui_all.h"
#include "u_all.h"

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
	static_cast<GUI_RadioButton*>(getChild("Options.Sharp"))->connectChanged(this, &Options::handleClick);
	static_cast<GUI_RadioButton*>(getChild("Options.Smooth"))->connectChanged(this, &Options::handleClick);
	static_cast<GUI_RadioButton*>(getChild("Options.SharpFit"))->connectChanged(this, &Options::handleClick);
	static_cast<GUI_RadioButton*>(getChild("Options.Crt"))->connectChanged(this, &Options::handleClick);
	static_cast<GUI_Button*>(getChild("Options.CrtSettings"))->connectClicked(this, &Options::handleClick);
	static_cast<GUI_ScrollBar*>(getChild("CrtOptions.Scan"))->connectChanged(this, &Options::handleClick);
	static_cast<GUI_ScrollBar*>(getChild("CrtOptions.Curve"))->connectChanged(this, &Options::handleClick);
	static_cast<GUI_ScrollBar*>(getChild("CrtOptions.Bloom"))->connectChanged(this, &Options::handleClick);
	static_cast<GUI_ScrollBar*>(getChild("CrtOptions.Flicker"))->connectChanged(this, &Options::handleClick);
	static_cast<GUI_ScrollBar*>(getChild("CrtOptions.ScanFlicker"))->connectChanged(this, &Options::handleClick);
	static_cast<GUI_ScrollBar*>(getChild("CrtOptions.Converge"))->connectChanged(this, &Options::handleClick);
	static_cast<GUI_Button*>(getChild("CrtOptions.Close"))->connectClicked(this, &Options::handleClick);
	static_cast<GUI_ListBox*>(getChild("Options.Actions"))->connectChanged(this, &Options::handleClick);
	static_cast<GUI_Button*>(getChild("Options.ResetSelected"))->connectClicked(this, &Options::handleClick);
	static_cast<GUI_Button*>(getChild("Options.ResetAll"))->connectClicked(this, &Options::handleClick);
	static_cast<GUI_Button*>(getChild("Options.PrimaryKey"))->connectClicked(this, &Options::handleClick);
	static_cast<GUI_Button*>(getChild("Options.SecondaryKey"))->connectClicked(this, &Options::handleClick);

	// enter the actions
	GUI_ListBox* p_actions = static_cast<GUI_ListBox*>(getChild("Options.Actions"));
	const std::vector<Action*>& actions = Engine::inst().getActionsVector();
	for(std::vector<Action*>::const_iterator it = actions.begin();
		it != actions.end();
		++it)
	{
		p_actions->addItem(GUI_ListBox::ListItem((*it)->name.c_str()));
	}

	p_focusWhenClosed = 0;
	grabButton = "";
	grabAction = "";
	changed = false;
}

Options::~Options()
{
}

void Options::show(GUI_Element* p_focusWhenClosed)
{
	this->p_focusWhenClosed = p_focusWhenClosed;

	// Every click below sets it, the closing ones included, so it starts
	// fresh here: Cancel reloads config.xml only when something was touched.
	changed = false;

	Engine& engine = Engine::inst();

	// set the current language
	if(engine.getLanguage() == "en") static_cast<GUI_RadioButton*>(getChild("Options.English"))->setChecked();
	else if(engine.getLanguage() == "de") static_cast<GUI_RadioButton*>(getChild("Options.German"))->setChecked();

	// set the current sound volume
	static_cast<GUI_ScrollBar*>(getChild("Options.SoundVolume"))->setScroll(static_cast<int>(100.0f * engine.getSoundVolume()));

	// set the current music volume
	static_cast<GUI_ScrollBar*>(getChild("Options.MusicVolume"))->setScroll(static_cast<int>(100.0f * engine.getMusicVolume()));

	// set the current details
	if(engine.getDetails() == 0) static_cast<GUI_RadioButton*>(getChild("Options.LowDetails"))->setChecked();
	else if(engine.getDetails() == 1) static_cast<GUI_RadioButton*>(getChild("Options.MediumDetails"))->setChecked();
	else if(engine.getDetails() == 2) static_cast<GUI_RadioButton*>(getChild("Options.HighDetails"))->setChecked();

	// The upscale filter. All four exist wherever the game starts at all, so
	// options.xml's layout stands and only the one in use is ticked. Each
	// radio button is named after its filter's getName(); nothing else maps
	// one to the other.
	static_cast<GUI_RadioButton*>(getChild(
		std::string("Options.") + engine.getUpscaler()->getName()))->setChecked();

	// Fetch the slider settings from the Engine, 0..1 as 0..100.
	U_Crt& crt = engine.getCrt();
	static_cast<GUI_ScrollBar*>(getChild("CrtOptions.Scan"))->setScroll(
		static_cast<int>(100.0f * crt.getScanline()));
	static_cast<GUI_ScrollBar*>(getChild("CrtOptions.Curve"))->setScroll(
		static_cast<int>(100.0f * crt.getCurvature()));
	static_cast<GUI_ScrollBar*>(getChild("CrtOptions.Bloom"))->setScroll(
		static_cast<int>(100.0f * crt.getBloom()));
	static_cast<GUI_ScrollBar*>(getChild("CrtOptions.Flicker"))->setScroll(
		static_cast<int>(100.0f * crt.getFlicker()));
	static_cast<GUI_ScrollBar*>(getChild("CrtOptions.ScanFlicker"))->setScroll(
		static_cast<int>(100.0f * crt.getScanFlicker()));
	static_cast<GUI_ScrollBar*>(getChild("CrtOptions.Converge"))->setScroll(
		static_cast<int>(100.0f * crt.getConvergence()));
	getChild("CrtOptions")->hide();

	// Start with no selection, and reset the key buttons by hand:
	// setSelection() reports only a real change, and handleClick() ignores
	// everything while the dialog is hidden.
	static_cast<GUI_ListBox*>(getChild("Options.Actions"))->setSelection(-1);
	static_cast<GUI_Button*>(getChild("Options.PrimaryKey"))->setTitle("");
	static_cast<GUI_Button*>(getChild("Options.SecondaryKey"))->setTitle("");
	static_cast<GUI_Button*>(getChild("Options.ResetSelected"))->deactivate();
	static_cast<GUI_Button*>(getChild("Options.PrimaryKey"))->deactivate();
	static_cast<GUI_Button*>(getChild("Options.SecondaryKey"))->deactivate();

	grabButton = "";
	grabAction = "";

	getChild("Options")->focus();
}

void Options::onKeyEvent(const SDL_KeyboardEvent& event)
{
	// A repeat is not a second command: after an Escape that ends a key grab,
	// its repeat would otherwise close the dialog straight away.
	if(event.type == SDL_KEYDOWN && isVisible() && !GUI::inst().isKeyRepeat())
	{
		const SDLKey key = event.keysym.sym;
		if(key == SDLK_ESCAPE || isReturnKey(key))
		{
			// Spent here: the game states ask Engine::wasKeyPressed() after
			// GUI::update(), and the main menu would quit on the same Escape.
			Engine::inst().consumeKeyPress(key);

			// The CRT settings window lies on top and gets the key first. Its
			// only button is OK, so both keys close it.
			if(getChild("CrtOptions")->isVisible()) handleClick(getChild("CrtOptions.Close"));
			else if(key == SDLK_ESCAPE)             handleClick(getChild("Options.Cancel"));
			else                                    handleClick(getChild("Options.OK"));
			return;
		}
	}

	GUI_Element::onKeyEvent(event);
}

void Options::onUpdate()
{
	if(grabButton.empty()) return;

	const int key = Engine::inst().pollKeyGrab();
	if(key == Engine::GRAB_WAITING) return;

	applyKeyGrab(key);
}

void Options::applyKeyGrab(int key)
{
	const std::string which(grabButton);
	const std::string what(grabAction);
	grabButton = "";
	grabAction = "";

	Engine& engine = Engine::inst();

	// GRAB_TIMED_OUT leaves the binding alone. GRAB_NO_KEY - Escape - clears
	// it, the only way to leave an action unbound.
	const Action* p_action = engine.getAction(what);
	if(key != Engine::GRAB_TIMED_OUT && p_action)
	{
		if(which == "PrimaryKey") engine.changeAction(what, key, p_action->secondary);
		else                      engine.changeAction(what, p_action->primary, key);
	}

	// Puts the two captions back to the binding - the old one after a timeout.
	handleClick(getChild("Options.Actions"));
}

void Options::handleClick(GUI_Element* p_element)
{
	std::string name = p_element->getName();
	Engine& engine = Engine::inst();

	if(isVisible())
	{
		// save the language
		if(static_cast<GUI_RadioButton*>(getChild("Options.German"))->isChecked()) engine.setLanguage("de");
		else if(static_cast<GUI_RadioButton*>(getChild("Options.English"))->isChecked()) engine.setLanguage("en");

		// save the sound volume
		engine.setSoundVolume((1.0f / 100.0f) * static_cast<GUI_ScrollBar*>(getChild("Options.SoundVolume"))->getScroll());

		// save the music volume
		engine.setMusicVolume((1.0f / 100.0f) * static_cast<GUI_ScrollBar*>(getChild("Options.MusicVolume"))->getScroll());

		// save the details
		if(static_cast<GUI_RadioButton*>(getChild("Options.LowDetails"))->isChecked()) engine.setDetails(0);
		else if(static_cast<GUI_RadioButton*>(getChild("Options.MediumDetails"))->isChecked()) engine.setDetails(1);
		else if(static_cast<GUI_RadioButton*>(getChild("Options.HighDetails"))->isChecked()) engine.setDetails(2);

		// Save the upscale filter. It takes effect with the next frame.
		const std::vector<Upscaler*>& upscalers = engine.getUpscalers();
		for(std::vector<Upscaler*>::const_iterator i = upscalers.begin(); i != upscalers.end(); ++i)
		{
			if(static_cast<GUI_RadioButton*>(getChild(
				std::string("Options.") + (*i)->getName()))->isChecked())
			{
				engine.setUpscaler(*i);
				break;
			}
		}

		// The CRT sliders take effect at once - dragging one has to show what it
		// does. Cancel takes them back through loadConfig().
		U_Crt& crt = engine.getCrt();
		crt.setScanline((1.0f / 100.0f) * static_cast<GUI_ScrollBar*>(getChild("CrtOptions.Scan"))->getScroll());
		crt.setCurvature((1.0f / 100.0f) * static_cast<GUI_ScrollBar*>(getChild("CrtOptions.Curve"))->getScroll());
		crt.setBloom((1.0f / 100.0f) * static_cast<GUI_ScrollBar*>(getChild("CrtOptions.Bloom"))->getScroll());
		crt.setFlicker((1.0f / 100.0f) * static_cast<GUI_ScrollBar*>(getChild("CrtOptions.Flicker"))->getScroll());
		crt.setScanFlicker((1.0f / 100.0f) * static_cast<GUI_ScrollBar*>(getChild("CrtOptions.ScanFlicker"))->getScroll());
		crt.setConvergence((1.0f / 100.0f) * static_cast<GUI_ScrollBar*>(getChild("CrtOptions.Converge"))->getScroll());

		if(name == "CrtSettings")
		{
			// The sliders make sense only together with the filter; the button
			// therefore switches it on there and then.
			static_cast<GUI_RadioButton*>(getChild("Options.Crt"))->check();
			engine.setUpscaler(&engine.getCrt());
			getChild("CrtOptions")->show();
			getChild("CrtOptions")->focus();
		}
		else if(name == "Close")
		{
			getChild("CrtOptions")->hide();
			getChild("Options")->focus();
		}
		else if(name == "Actions")
		{
			GUI_Button* p_primary = static_cast<GUI_Button*>(getChild("Options.PrimaryKey"));
			GUI_Button* p_secondary = static_cast<GUI_Button*>(getChild("Options.SecondaryKey"));
			GUI_ListBox* p_actions = static_cast<GUI_ListBox*>(getChild("Options.Actions"));
			const std::vector<Action*>& actions = Engine::inst().getActionsVector();

			GUI_Button* p_resetSelected = static_cast<GUI_Button*>(getChild("Options.ResetSelected"));

			// With no selection there is nothing to rebind and nothing to
			// reset; all three buttons hang off it.
			int selection = p_actions->getSelection();
			if(selection == -1)
			{
				p_primary->setTitle("");
				p_secondary->setTitle("");
				p_primary->deactivate();
				p_secondary->deactivate();
				p_resetSelected->deactivate();
			}
			else
			{
				const Action& action = *(actions[selection]);

				// Already localized, and an unassigned key says so itself, so
				// the name goes on the button as it is.
				p_primary->setTitle(engine.getVKDisplayName(action.primary));
				p_secondary->setTitle(engine.getVKDisplayName(action.secondary));
				p_primary->activate();
				p_secondary->activate();
				p_resetSelected->activate();
			}
		}
		else if(name == "PrimaryKey" || name == "SecondaryKey")
		{
			GUI_ListBox* p_actions = static_cast<GUI_ListBox*>(getChild("Options.Actions"));
			int selection = p_actions->getSelection();
			if(selection != -1)
			{
				// The button says it is waiting, and onUpdate() picks up the
				// result. The action is held by its name, not its number.
				static_cast<GUI_Button*>(p_element)->setTitle("$O_PRESS_KEY");

				grabButton = name;
				grabAction = Engine::inst().getActionsVector()[selection]->name;
				Engine::inst().beginKeyGrab();
			}
		}
		else if(name == "ResetSelected" || name == "ResetAll")
		{
			GUI_ListBox* p_actions = static_cast<GUI_ListBox*>(getChild("Options.Actions"));

			if(name == "ResetAll") Engine::inst().resetActions();
			else
			{
				// The button is disabled with no selection; the check stands here
				// anyway, because actions[selection] needs it regardless.
				const int selection = p_actions->getSelection();
				if(selection == -1) return;
				Engine::inst().resetAction(Engine::inst().getActionsVector()[selection]->name);
			}

			// The two key buttons show the selected action's binding and have to
			// follow.
			handleClick(p_actions);
		}
		else if(name == "OK")
		{
			getChild("CrtOptions")->hide();
			engine.saveConfig();

			hide();
			if(p_focusWhenClosed) p_focusWhenClosed->focus();
		}
		else if(name == "Cancel")
		{
			getChild("CrtOptions")->hide();
			if(changed) engine.loadConfig();

			hide();
			if(p_focusWhenClosed) p_focusWhenClosed->focus();
		}

		changed = true;
	}
}