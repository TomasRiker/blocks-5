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

	Engine& engine = Engine::inst();

	// set the current language
	if(engine.getLanguage() == "en") static_cast<GUI_RadioButton*>(getChild("Options.English"))->setChecked();
	else if(engine.getLanguage() == "de") static_cast<GUI_RadioButton*>(getChild("Options.German"))->setChecked();

	// set the current sound volume
	static_cast<GUI_ScrollBar*>(getChild("Options.SoundVolume"))->setScroll(static_cast<int>(100.0 * engine.getSoundVolume()));

	// set the current music volume
	static_cast<GUI_ScrollBar*>(getChild("Options.MusicVolume"))->setScroll(static_cast<int>(100.0 * engine.getMusicVolume()));

	// set the current details
	if(engine.getDetails() == 0) static_cast<GUI_RadioButton*>(getChild("Options.LowDetails"))->setChecked();
	else if(engine.getDetails() == 1) static_cast<GUI_RadioButton*>(getChild("Options.MediumDetails"))->setChecked();
	else if(engine.getDetails() == 2) static_cast<GUI_RadioButton*>(getChild("Options.HighDetails"))->setChecked();

	// The upscale filters, the best first. Without a shader SharpFit is not
	// there to be seen at all and the rest move up. The CRT filter comes last:
	// the three above it are upscalers sorted by quality, the fourth is a
	// matter of style. It needs the same shader and disappears without it just
	// the same.
	//
	// The order lives in the Engine, and the radio button is named after the
	// filter - "Options." + getName() is therefore not a convenience but the
	// one place that spells that mapping out.
	const std::vector<Upscaler*>& upscalers = engine.getUpscalers();

	// 50 is the top edge of the language flags beside them (options.xml,
	// Static3).
	int filterY = 50;
	for(std::vector<Upscaler*>::const_iterator i = upscalers.begin(); i != upscalers.end(); ++i)
	{
		const std::string element(std::string("Options.") + (*i)->getName());
		GUI_Element* p_button = getChild(element);
		// The label is an element of its own (<For> points back at the button)
		// and therefore has to move along with it.
		GUI_Element* p_label = getChild(element + "Label");
		if((*i)->isAvailable())
		{
			p_button->setPosition(Vec2i(p_button->getPosition().x, filterY));
			p_button->show();
			if(p_label)
			{
				p_label->setPosition(Vec2i(p_label->getPosition().x, filterY + 3));
				p_label->show();
			}
			filterY += 20;
		}
		else
		{
			p_button->hide();
			if(p_label) p_label->hide();
		}
	}

	// The button to the sliders slides in under the last visible entry.
	GUI_Element* p_crtSettings = getChild("Options.CrtSettings");
	if(engine.getCrt().isAvailable())
	{
		// After the loop filterY stands exactly one step below the last entry,
		// giving the button the same spacing as the buttons have between
		// themselves. 20 is the line pitch of the other dialogs.
		p_crtSettings->setPosition(Vec2i(p_crtSettings->getPosition().x, filterY));
		p_crtSettings->show();
	}
	else p_crtSettings->hide();

	static_cast<GUI_RadioButton*>(getChild(
		std::string("Options.") + engine.getEffectiveUpscaler()->getName()))->setChecked();

	// Fetch the slider settings from the Engine, 0..1 as 0..100.
	U_Crt& crt = engine.getCrt();
	static_cast<GUI_ScrollBar*>(getChild("CrtOptions.Scan"))->setScroll(
		static_cast<int>(100.0 * crt.getScanline()));
	static_cast<GUI_ScrollBar*>(getChild("CrtOptions.Curve"))->setScroll(
		static_cast<int>(100.0 * crt.getCurvature()));
	static_cast<GUI_ScrollBar*>(getChild("CrtOptions.Bloom"))->setScroll(
		static_cast<int>(100.0 * crt.getBloom()));
	static_cast<GUI_ScrollBar*>(getChild("CrtOptions.Flicker"))->setScroll(
		static_cast<int>(100.0 * crt.getFlicker()));
	static_cast<GUI_ScrollBar*>(getChild("CrtOptions.ScanFlicker"))->setScroll(
		static_cast<int>(100.0 * crt.getScanFlicker()));
	static_cast<GUI_ScrollBar*>(getChild("CrtOptions.Converge"))->setScroll(
		static_cast<int>(100.0 * crt.getConvergence()));
	getChild("CrtOptions")->hide();

	// Start with no selection. setSelection() reports only a real change: if
	// it already stood at -1, the branch below does not run.
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
	// A repeat is not a second command. That shows above all while the dialog
	// is waiting for a key for an action: the Escape cancels the wait, and the
	// repeat after it would close the dialog straight away.
	if(event.type == SDL_KEYDOWN && isVisible() && !GUI::inst().isKeyRepeat())
	{
		const SDLKey key = event.keysym.sym;
		if(key == SDLK_ESCAPE || key == SDLK_RETURN)
		{
			// The key is spent here. The game states ask Engine::wasKeyPressed()
			// alongside, and GUI::update() runs first - or the main menu would
			// see the same Escape and quit the game.
			Engine::inst().consumeKeyPress(key);

			// The CRT settings window lies on top, and the key therefore belongs
			// to it first. It has only OK - both keys close it.
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

	// GRAB_CANCELLED means Escape: the binding is left alone. GRAB_NO_KEY -
	// the time ran out - clears it; that is the only way to leave an action
	// unbound.
	const Action* p_action = engine.getAction(what);
	if(key != Engine::GRAB_CANCELLED && p_action)
	{
		if(which == "PrimaryKey") engine.changeAction(what, key, p_action->secondary);
		else                      engine.changeAction(what, p_action->primary, key);
	}

	// Puts the two captions back to the binding - the old one after a cancel.
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
		engine.setSoundVolume((1.0 / 100.0) * static_cast<GUI_ScrollBar*>(getChild("Options.SoundVolume"))->getScroll());

		// save the music volume
		engine.setMusicVolume((1.0 / 100.0) * static_cast<GUI_ScrollBar*>(getChild("Options.MusicVolume"))->getScroll());

		// save the details
		if(static_cast<GUI_RadioButton*>(getChild("Options.LowDetails"))->isChecked()) engine.setDetails(0);
		else if(static_cast<GUI_RadioButton*>(getChild("Options.MediumDetails"))->isChecked()) engine.setDetails(1);
		else if(static_cast<GUI_RadioButton*>(getChild("Options.HighDetails"))->isChecked()) engine.setDetails(2);

		// Save the upscale filter. It takes effect at once: the next frame
		// already reaches the screen through the new filter.
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
		crt.setScanline((1.0 / 100.0) * static_cast<GUI_ScrollBar*>(getChild("CrtOptions.Scan"))->getScroll());
		crt.setCurvature((1.0 / 100.0) * static_cast<GUI_ScrollBar*>(getChild("CrtOptions.Curve"))->getScroll());
		crt.setBloom((1.0 / 100.0) * static_cast<GUI_ScrollBar*>(getChild("CrtOptions.Bloom"))->getScroll());
		crt.setFlicker((1.0 / 100.0) * static_cast<GUI_ScrollBar*>(getChild("CrtOptions.Flicker"))->getScroll());
		crt.setScanFlicker((1.0 / 100.0) * static_cast<GUI_ScrollBar*>(getChild("CrtOptions.ScanFlicker"))->getScroll());
		crt.setConvergence((1.0 / 100.0) * static_cast<GUI_ScrollBar*>(getChild("CrtOptions.Converge"))->getScroll());

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
				const std::vector<VirtualKey>& vks = Engine::inst().getVKs();
				p_primary->setTitle(action.primary == -1 ? "$O_NOT_ASSIGNED" : vks[action.primary].name);
				p_secondary->setTitle(action.secondary == -1 ? "$O_NOT_ASSIGNED" : vks[action.secondary].name);
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
				// The button itself says what it is waiting for, and the wait runs
				// alongside from now on: onUpdate() picks up the result as soon as
				// it is there. What is held on to is the action's name, not its
				// number.
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