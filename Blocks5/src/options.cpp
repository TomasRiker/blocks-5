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
	static_cast<GUI_RadioButton*>(getChild("Options.Nearest"))->connectChanged(this, &Options::handleClick);
	static_cast<GUI_RadioButton*>(getChild("Options.Bilinear"))->connectChanged(this, &Options::handleClick);
	static_cast<GUI_RadioButton*>(getChild("Options.SharpFit"))->connectChanged(this, &Options::handleClick);
	static_cast<GUI_RadioButton*>(getChild("Options.Crt"))->connectChanged(this, &Options::handleClick);
	static_cast<GUI_Button*>(getChild("Options.CrtSettings"))->connectClicked(this, &Options::handleClick);
	static_cast<GUI_ScrollBar*>(getChild("CrtOptions.CrtScan"))->connectChanged(this, &Options::handleClick);
	static_cast<GUI_ScrollBar*>(getChild("CrtOptions.CrtCurve"))->connectChanged(this, &Options::handleClick);
	static_cast<GUI_ScrollBar*>(getChild("CrtOptions.CrtBloom"))->connectChanged(this, &Options::handleClick);
	static_cast<GUI_ScrollBar*>(getChild("CrtOptions.CrtFlicker"))->connectChanged(this, &Options::handleClick);
	static_cast<GUI_Button*>(getChild("CrtOptions.CrtClose"))->connectClicked(this, &Options::handleClick);
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
	if(engine.getLanguage() == "en") static_cast<GUI_RadioButton*>(getChild("Options.English"))->setChecked();
	else if(engine.getLanguage() == "de") static_cast<GUI_RadioButton*>(getChild("Options.German"))->setChecked();

	// aktuelle Sound-Lautstärke setzen
	static_cast<GUI_ScrollBar*>(getChild("Options.SoundVolume"))->setScroll(static_cast<int>(100.0 * engine.getSoundVolume()));

	// aktuelle Musik-Lautstärke setzen
	static_cast<GUI_ScrollBar*>(getChild("Options.MusicVolume"))->setScroll(static_cast<int>(100.0 * engine.getMusicVolume()));

	// aktuelle Details setzen
	if(engine.getDetails() == 0) static_cast<GUI_RadioButton*>(getChild("Options.LowDetails"))->setChecked();
	else if(engine.getDetails() == 1) static_cast<GUI_RadioButton*>(getChild("Options.MediumDetails"))->setChecked();
	else if(engine.getDetails() == 2) static_cast<GUI_RadioButton*>(getChild("Options.HighDetails"))->setChecked();

	// Skalierungsfilter, von oben nach unten das Beste zuerst. Ohne Shader gibt
	// es "Scharf, angepasst" gar nicht erst zu sehen - anzubieten, was die
	// Maschine nicht kann, wäre gelogen -, und die übrigen rücken nach oben
	// nach, damit keine Lücke bleibt.
	// "Röhrenmonitor" steht zuletzt: die drei darüber sind Skalierer und nach
	// Güte sortiert, der Vierte ist eine Stilfrage und gehört nicht in dieselbe
	// Reihenfolge. Er braucht denselben Shader wie "Scharf, angepasst" und
	// verschwindet ohne ihn genauso.
	const char* pp_filterNames[4] =
	{
		"Options.SharpFit", "Options.Nearest", "Options.Bilinear", "Options.Crt"
	};
	const bool available[4] = { engine.canUseSharpFit(), true, true, engine.canUseCrt() };

	// 50 ist die Oberkante der Sprachflaggen daneben (options.xml, Static3).
	int filterY = 50;
	for(int i = 0; i < 4; i++)
	{
		GUI_Element* p_button = getChild(pp_filterNames[i]);
		// Die Beschriftung ist ein eigenes Element (<For> zeigt zurueck auf den
		// Knopf), also muss sie mitgehen.
		GUI_Element* p_label = getChild(std::string(pp_filterNames[i]) + "Label");
		if(available[i])
		{
			p_button->setPosition(Vec2i(p_button->getPosition().x, filterY));
			p_button->show();
			if(p_label)
			{
				p_label->setPosition(Vec2i(p_label->getPosition().x, filterY + 3));
				p_label->show();
			}
			filterY += 19;
		}
		else
		{
			p_button->hide();
			if(p_label) p_label->hide();
		}
	}

	// Der Knopf zu den Reglern rutscht unter den letzten sichtbaren Eintrag.
	GUI_Element* p_crtSettings = getChild("Options.CrtSettings");
	if(engine.canUseCrt())
	{
		// filterY steht nach der Schleife genau einen Schritt unter dem letzten
		// Eintrag, der Knopf bekommt also denselben Abstand wie die Knoepfe
		// untereinander.
		p_crtSettings->setPosition(Vec2i(p_crtSettings->getPosition().x, filterY));
		p_crtSettings->show();
	}
	else p_crtSettings->hide();

	switch(engine.getEffectiveUpscaleFilter())
	{
	case Engine::UF_NEAREST:    static_cast<GUI_RadioButton*>(getChild("Options.Nearest"))->setChecked(); break;
	case Engine::UF_SHARP_FIT:  static_cast<GUI_RadioButton*>(getChild("Options.SharpFit"))->setChecked(); break;
	case Engine::UF_CRT:        static_cast<GUI_RadioButton*>(getChild("Options.Crt"))->setChecked(); break;
	default:                    static_cast<GUI_RadioButton*>(getChild("Options.Bilinear"))->setChecked(); break;
	}

	// Reglerstellungen aus der Engine holen, 0..1 als 0..100.
	static_cast<GUI_ScrollBar*>(getChild("CrtOptions.CrtScan"))->setScroll(
		static_cast<int>(100.0 * engine.getCrtScanline()));
	static_cast<GUI_ScrollBar*>(getChild("CrtOptions.CrtCurve"))->setScroll(
		static_cast<int>(100.0 * engine.getCrtCurvature()));
	static_cast<GUI_ScrollBar*>(getChild("CrtOptions.CrtBloom"))->setScroll(
		static_cast<int>(100.0 * engine.getCrtBloom()));
	static_cast<GUI_ScrollBar*>(getChild("CrtOptions.CrtFlicker"))->setScroll(
		static_cast<int>(100.0 * engine.getCrtFlicker()));
	getChild("CrtOptions")->hide();

	static_cast<GUI_ListBox*>(getChild("Options.Actions"))->setSelection(-1);
	static_cast<GUI_Button*>(getChild("Options.PrimaryKey"))->setTitle("");
	static_cast<GUI_Button*>(getChild("Options.SecondaryKey"))->setTitle("");

	getChild("Options")->focus();
}

void Options::onKeyEvent(const SDL_KeyboardEvent& event)
{
	if(event.type == SDL_KEYDOWN && isVisible())
	{
		const SDLKey key = event.keysym.sym;
		if(key == SDLK_ESCAPE || key == SDLK_RETURN)
		{
			// Die Taste ist hiermit verbraucht. Die Spielzustaende fragen
			// daneben Engine::wasKeyPressed() ab, und GUI::update() laeuft
			// vorher - ohne das wuerde das Hauptmenue dasselbe Escape sehen,
			// mit dem dieser Dialog sich gerade geschlossen hat, und das Spiel
			// beenden.
			Engine::inst().consumeKeyPress(key);

			// Das Roehrenfenster liegt oben drauf, also gehoert ihm die Taste
			// zuerst. Es hat nur OK - beide Tasten schliessen es.
			if(getChild("CrtOptions")->isVisible()) handleClick(getChild("CrtOptions.CrtClose"));
			else if(key == SDLK_ESCAPE)             handleClick(getChild("Options.Cancel"));
			else                                    handleClick(getChild("Options.OK"));
			return;
		}
	}

	GUI_Element::onKeyEvent(event);
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

		// Skalierungsfilter speichern. Wirkt sofort, das nächste Bild kommt
		// schon durch den neuen Filter auf den Schirm.
		if(static_cast<GUI_RadioButton*>(getChild("Options.Nearest"))->isChecked()) engine.setUpscaleFilter(Engine::UF_NEAREST);
		else if(static_cast<GUI_RadioButton*>(getChild("Options.Bilinear"))->isChecked()) engine.setUpscaleFilter(Engine::UF_BILINEAR);
		else if(static_cast<GUI_RadioButton*>(getChild("Options.SharpFit"))->isChecked()) engine.setUpscaleFilter(Engine::UF_SHARP_FIT);
		else if(static_cast<GUI_RadioButton*>(getChild("Options.Crt"))->isChecked()) engine.setUpscaleFilter(Engine::UF_CRT);

		// Die beiden Röhrenregler wirken sofort - beim Schieben soll man ja
		// sehen, was sie tun. Zurückgenommen werden sie von Abbrechen, das
		// über loadConfig() auch <Crt> wieder liest.
		engine.setCrtScanline((1.0 / 100.0) * static_cast<GUI_ScrollBar*>(getChild("CrtOptions.CrtScan"))->getScroll());
		engine.setCrtCurvature((1.0 / 100.0) * static_cast<GUI_ScrollBar*>(getChild("CrtOptions.CrtCurve"))->getScroll());
		engine.setCrtBloom((1.0 / 100.0) * static_cast<GUI_ScrollBar*>(getChild("CrtOptions.CrtBloom"))->getScroll());
		engine.setCrtFlicker((1.0 / 100.0) * static_cast<GUI_ScrollBar*>(getChild("CrtOptions.CrtFlicker"))->getScroll());

		if(name == "CrtSettings")
		{
			// Die Regler ergeben nur zusammen mit dem Filter einen Sinn, also
			// schaltet der Knopf ihn gleich mit ein.
			static_cast<GUI_RadioButton*>(getChild("Options.Crt"))->check();
			engine.setUpscaleFilter(Engine::UF_CRT);
			getChild("CrtOptions")->show();
			getChild("CrtOptions")->focus();
		}
		else if(name == "CrtClose")
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