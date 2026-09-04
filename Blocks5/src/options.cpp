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

	// aktuelle Sprache setzen
	if(engine.getLanguage() == "en") static_cast<GUI_RadioButton*>(getChild("Options.English"))->setChecked();
	else if(engine.getLanguage() == "de") static_cast<GUI_RadioButton*>(getChild("Options.German"))->setChecked();

	// aktuelle Sound-Lautstaerke setzen
	static_cast<GUI_ScrollBar*>(getChild("Options.SoundVolume"))->setScroll(static_cast<int>(100.0 * engine.getSoundVolume()));

	// aktuelle Musik-Lautstaerke setzen
	static_cast<GUI_ScrollBar*>(getChild("Options.MusicVolume"))->setScroll(static_cast<int>(100.0 * engine.getMusicVolume()));

	// aktuelle Details setzen
	if(engine.getDetails() == 0) static_cast<GUI_RadioButton*>(getChild("Options.LowDetails"))->setChecked();
	else if(engine.getDetails() == 1) static_cast<GUI_RadioButton*>(getChild("Options.MediumDetails"))->setChecked();
	else if(engine.getDetails() == 2) static_cast<GUI_RadioButton*>(getChild("Options.HighDetails"))->setChecked();

	// Skalierungsfilter, das Beste zuerst. Ohne Shader gibt es "Scharf,
	// angepasst" gar nicht erst zu sehen und die uebrigen ruecken nach oben.
	// "Roehrenmonitor" steht zuletzt: die drei darueber sind Skalierer und nach
	// Guete sortiert, der Vierte ist eine Stilfrage. Er braucht denselben
	// Shader und verschwindet ohne ihn genauso.
	const char* pp_filterNames[4] =
	{
		"Options.SharpFit", "Options.Sharp", "Options.Smooth", "Options.Crt"
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
			filterY += 20;
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
		// untereinander. 20 ist der Zeilenabstand der uebrigen Dialoge.
		p_crtSettings->setPosition(Vec2i(p_crtSettings->getPosition().x, filterY));
		p_crtSettings->show();
	}
	else p_crtSettings->hide();

	switch(engine.getEffectiveUpscaleFilter())
	{
	case Engine::UF_NEAREST:    static_cast<GUI_RadioButton*>(getChild("Options.Sharp"))->setChecked(); break;
	case Engine::UF_SHARP_FIT:  static_cast<GUI_RadioButton*>(getChild("Options.SharpFit"))->setChecked(); break;
	case Engine::UF_CRT:        static_cast<GUI_RadioButton*>(getChild("Options.Crt"))->setChecked(); break;
	default:                    static_cast<GUI_RadioButton*>(getChild("Options.Smooth"))->setChecked(); break;
	}

	// Reglerstellungen aus der Engine holen, 0..1 als 0..100.
	static_cast<GUI_ScrollBar*>(getChild("CrtOptions.Scan"))->setScroll(
		static_cast<int>(100.0 * engine.getCrtScanline()));
	static_cast<GUI_ScrollBar*>(getChild("CrtOptions.Curve"))->setScroll(
		static_cast<int>(100.0 * engine.getCrtCurvature()));
	static_cast<GUI_ScrollBar*>(getChild("CrtOptions.Bloom"))->setScroll(
		static_cast<int>(100.0 * engine.getCrtBloom()));
	static_cast<GUI_ScrollBar*>(getChild("CrtOptions.Flicker"))->setScroll(
		static_cast<int>(100.0 * engine.getCrtFlicker()));
	static_cast<GUI_ScrollBar*>(getChild("CrtOptions.ScanFlicker"))->setScroll(
		static_cast<int>(100.0 * engine.getCrtScanFlicker()));
	static_cast<GUI_ScrollBar*>(getChild("CrtOptions.Converge"))->setScroll(
		static_cast<int>(100.0 * engine.getCrtConvergence()));
	getChild("CrtOptions")->hide();

	// Ohne Auswahl beginnen. setSelection() meldet sich nur bei einer echten
	// Aenderung, stand es also schon auf -1, kommt der Zweig unten nicht.
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
	// Eine Wiederholung ist kein zweiter Befehl. Das faellt vor allem auf,
	// wenn der Dialog gerade auf eine Taste fuer eine Aktion wartet: das
	// Escape bricht das Warten ab, und die Wiederholung danach schloesse
	// gleich noch den Dialog.
	if(event.type == SDL_KEYDOWN && isVisible() && !GUI::inst().isKeyRepeat())
	{
		const SDLKey key = event.keysym.sym;
		if(key == SDLK_ESCAPE || key == SDLK_RETURN)
		{
			// Die Taste ist hiermit verbraucht. Die Spielzustaende fragen daneben
			// Engine::wasKeyPressed() ab, und GUI::update() laeuft vorher - sonst saehe
			// das Hauptmenue dasselbe Escape und beendete das Spiel.
			Engine::inst().consumeKeyPress(key);

			// Das Roehrenfenster liegt oben drauf, also gehoert ihm die Taste
			// zuerst. Es hat nur OK - beide Tasten schliessen es.
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

	// GRAB_CANCELLED heisst Escape: die Belegung bleibt, wie sie war.
	// GRAB_NO_KEY - die Zeit ist abgelaufen - raeumt sie weg; das ist der
	// einzige Weg, eine Aktion unbelegt zu lassen.
	const Action* p_action = engine.getAction(what);
	if(key != Engine::GRAB_CANCELLED && p_action)
	{
		if(which == "PrimaryKey") engine.changeAction(what, key, p_action->secondary);
		else                      engine.changeAction(what, p_action->primary, key);
	}

	// Setzt die beiden Aufschriften wieder auf die Belegung - die alte, wenn
	// abgebrochen wurde.
	handleClick(getChild("Options.Actions"));
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

		// Sound-Lautstaerke speichern
		engine.setSoundVolume((1.0 / 100.0) * static_cast<GUI_ScrollBar*>(getChild("Options.SoundVolume"))->getScroll());

		// Musik-Lautstaerke speichern
		engine.setMusicVolume((1.0 / 100.0) * static_cast<GUI_ScrollBar*>(getChild("Options.MusicVolume"))->getScroll());

		// Details speichern
		if(static_cast<GUI_RadioButton*>(getChild("Options.LowDetails"))->isChecked()) engine.setDetails(0);
		else if(static_cast<GUI_RadioButton*>(getChild("Options.MediumDetails"))->isChecked()) engine.setDetails(1);
		else if(static_cast<GUI_RadioButton*>(getChild("Options.HighDetails"))->isChecked()) engine.setDetails(2);

		// Skalierungsfilter speichern. Wirkt sofort, das naechste Bild kommt
		// schon durch den neuen Filter auf den Schirm.
		if(static_cast<GUI_RadioButton*>(getChild("Options.Sharp"))->isChecked()) engine.setUpscaleFilter(Engine::UF_NEAREST);
		else if(static_cast<GUI_RadioButton*>(getChild("Options.Smooth"))->isChecked()) engine.setUpscaleFilter(Engine::UF_BILINEAR);
		else if(static_cast<GUI_RadioButton*>(getChild("Options.SharpFit"))->isChecked()) engine.setUpscaleFilter(Engine::UF_SHARP_FIT);
		else if(static_cast<GUI_RadioButton*>(getChild("Options.Crt"))->isChecked()) engine.setUpscaleFilter(Engine::UF_CRT);

		// Die Roehrenregler wirken sofort - beim Schieben soll man sehen, was sie
		// tun. Abbrechen nimmt sie ueber loadConfig() zurueck.
		engine.setCrtScanline((1.0 / 100.0) * static_cast<GUI_ScrollBar*>(getChild("CrtOptions.Scan"))->getScroll());
		engine.setCrtCurvature((1.0 / 100.0) * static_cast<GUI_ScrollBar*>(getChild("CrtOptions.Curve"))->getScroll());
		engine.setCrtBloom((1.0 / 100.0) * static_cast<GUI_ScrollBar*>(getChild("CrtOptions.Bloom"))->getScroll());
		engine.setCrtFlicker((1.0 / 100.0) * static_cast<GUI_ScrollBar*>(getChild("CrtOptions.Flicker"))->getScroll());
		engine.setCrtScanFlicker((1.0 / 100.0) * static_cast<GUI_ScrollBar*>(getChild("CrtOptions.ScanFlicker"))->getScroll());
		engine.setCrtConvergence((1.0 / 100.0) * static_cast<GUI_ScrollBar*>(getChild("CrtOptions.Converge"))->getScroll());

		if(name == "CrtSettings")
		{
			// Die Regler ergeben nur zusammen mit dem Filter einen Sinn, also
			// schaltet der Knopf ihn gleich mit ein.
			static_cast<GUI_RadioButton*>(getChild("Options.Crt"))->check();
			engine.setUpscaleFilter(Engine::UF_CRT);
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

			// Ohne Auswahl gibt es nichts umzubelegen und nichts
			// zurueckzusetzen; alle drei Knoepfe haengen an ihr.
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
				// Der Knopf sagt selbst, worauf er wartet, und das Warten laeuft von jetzt
				// an nebenher: onUpdate() holt das Ergebnis ab, sobald es da ist.
				// Festgehalten wird der Name der Aktion, nicht ihre Nummer.
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
				// Der Knopf ist ohne Auswahl abgeschaltet; die Pruefung steht trotzdem
				// hier, weil actions[selection] sie ohnehin braucht.
				const int selection = p_actions->getSelection();
				if(selection == -1) return;
				Engine::inst().resetAction(Engine::inst().getActionsVector()[selection]->name);
			}

			// Die beiden Tastenknoepfe zeigen die Belegung der ausgewaehlten
			// Aktion und muessen nachziehen.
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