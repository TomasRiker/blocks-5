#include "pch.h"
#include "gs_menu.h"
#include "gui.h"
#include "gui_all.h"
#include "cf_all.h"
#include "texture.h"
#include "options.h"
#include "help.h"
#include "filesystem.h"
#include "transfer.h"
#ifdef _WIN32
#include <shellapi.h>
#endif
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

extern const char* p_localVersion;

namespace
{
	// demo1.dat haelt rohe Tastennummern fest - die von SDL 1.2, mit denen die
	// Aufnahme seinerzeit unter Windows entstanden ist. Emscriptens SDL zaehlt
	// anders: SDLK_LEFT ist dort 1104 und nicht 276. Unuebersetzt schrieb die
	// Demo also in Tastenplaetze, die niemand liest - Bob stand im Titelbild
	// still, bis er einschlief.
	//
	// Die Tabelle bildet die aufgezeichnete Zahl auf die Konstante ab, die
	// *dieser* Build meint. Unter Windows ist das die Identitaet, denn dort
	// sind die Konstanten genau die aufgezeichneten Zahlen; im Browser ist es
	// die Umrechnung. Mehr als diese acht Tasten kommen in der Aufnahme nicht
	// vor - nachgezaehlt, es sind genau diese.
	struct RecordedKey
	{
		uint recorded;
		int  key;
	};

	const RecordedKey p_recordedKeys[] =
	{
		{  9, SDLK_TAB   }, {273, SDLK_UP    }, {274, SDLK_DOWN  }, {275, SDLK_RIGHT },
		{276, SDLK_LEFT  }, {278, SDLK_HOME  }, {304, SDLK_LSHIFT}, {306, SDLK_LCTRL }
	};

	uint translateRecordedKey(uint recorded)
	{
		for(uint i = 0; i < sizeof(p_recordedKeys) / sizeof(p_recordedKeys[0]); i++)
		{
			if(p_recordedKeys[i].recorded == recorded) return p_recordedKeys[i].key;
		}

		return recorded;
	}
}

GS_Menu::GS_Menu() : GameState("GS_Menu"), engine(Engine::inst()), titleLevelXML("")
{
	p_clouds = 0;
	p_background = 0;
	p_titleLevel = 0;
	levelSaved = false;
	pendingExportKind = 0;
	pendingExport = false;
	pendingDeleteKind = 0;
}

GS_Menu::~GS_Menu()
{
}

void GS_Menu::onRender()
{
	// Farbuebergang rendern
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
	// Die Dateidialoge laufen hier, nicht im Klick-Handler: unter Windows
	// sind sie modal und wuerden sonst eine zweite Nachrichtenschleife mitten
	// in der Ereignisverteilung der GUI starten.
	pollImport();
	pollExport();


#ifdef __EMSCRIPTEN__
	Uint8* p_keyStates = SDL_GetKeyboardState(0);
#else
	Uint8* p_keyStates = SDL_GetKeyState(0);
#endif
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

	// Escape beendet das Spiel. Das muss *vor* dem Abspielen der Demo stehen:
	// gleich darunter werden alle Tastenplaetze geleert und mit den
	// aufgezeichneten Tastendruecken aus demo1.dat gefuellt, damit sich der
	// Titellevel von selbst spielt. Ab da liest wasKeyPressed() die Aufnahme
	// und nicht mehr die Tastatur - deshalb fragen auch die beiden Abfragen
	// darueber SDL direkt. Der Escape-Druck von oben steht hier noch.
	//
	// Nicht, wenn die Spendenfrage offen ist - die hat ihre eigenen Knoepfe -,
	// und nicht, wenn Optionen oder Hilfe die Taste eben selbst benutzt haben:
	// die melden sie mit consumeKeyPress() ab, weil GUI::update() vor
	// onUpdate() laeuft und sie sonst im selben Bild schliessen *und* das
	// Spiel beenden wuerden.
	if(engine.wasKeyPressed(SDLK_ESCAPE) &&
	   !gui["Menu.DonatePane"]->isVisible() &&
	   !gui["Menu.CrtPane"]->isVisible())
	{
		// Der Manager und seine Rueckfrage nehmen die Taste fuer sich, von
		// oben nach unten. Bisher tat der Export-Dialog das nicht: ein
		// Escape bei offenem Fenster beendete das Spiel.
		if(gui["Menu.ConfirmPane"]->isVisible())
		{
			handleClick(gui["Menu.ConfirmPane.Confirm.No"]);
		}
		else if(gui["Menu.ManagerPane"]->isVisible())
		{
			handleClick(gui["Menu.ManagerPane.Manager.Close"]);
		}
		else
		{
			SDL_Event quitEvent;
			quitEvent.type = SDL_QUIT;
			SDL_PushEvent(&quitEvent);
		}
	}

	// Ab hier gehoert die Tastatur der Demo.
	// SDLK_LAST, nicht 512: Emscriptens SDL zaehlt bis 1536, und die Pfeiltasten
	// liegen dort jenseits von 512. Ungeloescht bliebe eine davon fuer immer
	// gedrueckt, sobald die Demo sie einmal gesetzt hat.
	for(int i = 0; i < SDLK_LAST; i++)
	{
		engine.setKeyData(static_cast<SDLKey>(i), 0);
	}

	std::unordered_map<uint, std::list<uint> >::const_iterator i = keyData.find(time - 500);
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

	// Menue erzeugen
	gui.getRoot()->load("menu.xml");

	static_cast<GUI_StaticText*>(gui["Menu.Version"])->setText(p_localVersion);
	static_cast<GUI_Button*>(gui["Menu.StartGame"])->connectClicked(this, &GS_Menu::handleClick);
	static_cast<GUI_Button*>(gui["Menu.LevelEditor"])->connectClicked(this, &GS_Menu::handleClick);
	static_cast<GUI_Button*>(gui["Menu.CampaignEditor"])->connectClicked(this, &GS_Menu::handleClick);
	static_cast<GUI_Button*>(gui["Menu.Options"])->connectClicked(this, &GS_Menu::handleClick);
	static_cast<GUI_Button*>(gui["Menu.Help"])->connectClicked(this, &GS_Menu::handleClick);
	static_cast<GUI_Button*>(gui["Menu.Quit"])->connectClicked(this, &GS_Menu::handleClick);
	static_cast<GUI_Button*>(gui["Menu.Website"])->connectClicked(this, &GS_Menu::handleClick);
	static_cast<GUI_Button*>(gui["Menu.Donate"])->connectClicked(this, &GS_Menu::handleClick);
	static_cast<GUI_Button*>(gui["Menu.DonatePane.Donate.NoThanks"])->connectClicked(this, &GS_Menu::handleClick);
	static_cast<GUI_Button*>(gui["Menu.DonatePane.Donate.Donate"])->connectClicked(this, &GS_Menu::handleClick);
	static_cast<GUI_Button*>(gui["Menu.CrtPane.Crt.NoThanks"])->connectClicked(this, &GS_Menu::handleClick);
	static_cast<GUI_Button*>(gui["Menu.CrtPane.Crt.TryIt"])->connectClicked(this, &GS_Menu::handleClick);
	static_cast<GUI_Button*>(gui["Menu.Manager"])->connectClicked(this, &GS_Menu::handleClick);
	static_cast<GUI_Button*>(gui["Menu.ManagerPane.Manager.Refresh"])->connectClicked(this, &GS_Menu::handleClick);
	static_cast<GUI_Button*>(gui["Menu.ManagerPane.Manager.Import"])->connectClicked(this, &GS_Menu::handleClick);
	static_cast<GUI_Button*>(gui["Menu.ManagerPane.Manager.Export"])->connectClicked(this, &GS_Menu::handleClick);
	static_cast<GUI_Button*>(gui["Menu.ManagerPane.Manager.Delete"])->connectClicked(this, &GS_Menu::handleClick);
	static_cast<GUI_Button*>(gui["Menu.ManagerPane.Manager.Close"])->connectClicked(this, &GS_Menu::handleClick);
	static_cast<GUI_RadioButton*>(gui["Menu.ManagerPane.Manager.KindLevel"])->connectChanged(this, &GS_Menu::handleClick);
	static_cast<GUI_RadioButton*>(gui["Menu.ManagerPane.Manager.KindCampaign"])->connectChanged(this, &GS_Menu::handleClick);
	static_cast<GUI_RadioButton*>(gui["Menu.ManagerPane.Manager.KindMusic"])->connectChanged(this, &GS_Menu::handleClick);
	static_cast<GUI_RadioButton*>(gui["Menu.ManagerPane.Manager.KindSkin"])->connectChanged(this, &GS_Menu::handleClick);

	// Ausgeben und Loeschen haengen an der Auswahl, also muss die Liste sagen,
	// wenn sie sich aendert.
	static_cast<GUI_ListBox*>(gui["Menu.ManagerPane.Manager.Items"])->connectChanged(this, &GS_Menu::handleClick);

	static_cast<GUI_Button*>(gui["Menu.ConfirmPane.Confirm.Yes"])->connectClicked(this, &GS_Menu::handleClick);
	static_cast<GUI_Button*>(gui["Menu.ConfirmPane.Confirm.No"])->connectClicked(this, &GS_Menu::handleClick);

	FileSystem& fs = FileSystem::inst();

	// Einmalig auf den CRT-Filter hinweisen: beim ersten Start nach der
	// Installation und ebenso beim ersten nach einem Update, denn die
	// Merkdatei gibt es vor 1.2.0 nirgends. Nur, wenn die Maschine den Filter
	// ueberhaupt darstellen kann - sonst waere es ein Angebot, das der
	// Optionsdialog gar nicht erst auffuehrt.
	// Wer ihn schon eingeschaltet hat, muss nicht gefragt werden. Vorkommen
	// kann das eigentlich nur, wenn die config.xml von woanders stammt.
	const std::string crtOfferedPath(fs.getAppHomeDirectory() + ".crt_offered");
	const bool offerCrt = engine.canUseCrt() &&
						  engine.getUpscaleFilter() != Engine::UF_CRT &&
						  !fs.fileExists(crtOfferedPath);
	if(offerCrt) gui["Menu.CrtPane.Crt"]->focus();

	// Wann wurde zuletzt nach einer Spende gefragt?
	const std::string lastAskedForDonationStr = fs.fileExists(fs.getAppHomeDirectory() + ".donation_asked") ? fs.readStringFromFile(fs.getAppHomeDirectory() + ".donation_asked") : "";
	if(lastAskedForDonationStr != "disable")
	{
		const uint lastAskedForDonation = static_cast<uint>(atoi(lastAskedForDonationStr.c_str()));
		const uint timePlayed = engine.getTimePlayed();

		// Beide Zahlen sind vorzeichenlos. Steht in .donation_asked eine
		// groessere als in .time_played, lief die Differenz ueber und ergab
		// etwas Riesiges - das Spendenfenster kam dann bei jedem Start. Im
		// Browser war das der Normalfall: dort schrieb niemand .time_played,
		// die gespielte Zeit fing also jedesmal wieder bei null an, waehrend
		// .donation_asked vom ersten Mal her stehenblieb.
		// Nicht beides auf einmal: der CRT-Hinweis kommt genau einmal, die
		// Spendenfrage naechstes Mal wieder.
		if(!offerCrt &&
		   timePlayed >= lastAskedForDonation &&
		   timePlayed - lastAskedForDonation >= 60 * (60 * 60 * 3))
		{
			gui["Menu.DonatePane.Donate"]->focus();
		}
	}

	p_options = new Options(0);
	p_help = new Help(0);

	// Keyboard-Daten fuer die Demo laden
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
			keyData[t].push_back(translateRecordedKey(key));
			keyData[t].push_back(data);
		}
	}
	FileSystem::inst().closeFile(p_file);

	SDL_ShowCursor(1);
}

void GS_Menu::onLeave(const ParameterBlock& context)
{
	// Bilder loeschen
	p_clouds->release();
	p_clouds = 0;
	p_background->release();
	p_background = 0;

	if(p_titleLevel) delete p_titleLevel;
	p_titleLevel = 0;
	levelSaved = false;

	// Einen noch offenen Dateidialog aufgeben, sonst belegt er den Kanal
	// weiter, bis der Browser ihn nach fuenf Minuten selbst verwirft.
	Transfer::abandonImport();

	// Menue loeschen
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
	// Titel-Level speichern und loeschen
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
	else if(name == "Menu.Manager")
	{
		openManager();
	}
	else if(name == "Menu.ManagerPane.Manager.KindLevel" ||
			name == "Menu.ManagerPane.Manager.KindCampaign" ||
			name == "Menu.ManagerPane.Manager.KindMusic" ||
			name == "Menu.ManagerPane.Manager.KindSkin" ||
			name == "Menu.ManagerPane.Manager.Refresh")
	{
		// Beim Wechsel der Art und auf Wunsch neu einlesen: waehrend das
		// Fenster offensteht, kann sich das Verzeichnis geaendert haben.
		refreshManagerList();
	}
	else if(name == "Menu.ManagerPane.Manager.Items")
	{
		// Die Auswahl hat gewechselt - Ausgeben und Loeschen richten sich
		// danach.
		updateManagerButtons();
	}
	else if(name == "Menu.ManagerPane.Manager.Close")
	{
		gui["Menu.ManagerPane"]->hide();
		gui["Menu"]->focus();
	}
	else if(name == "Menu.ManagerPane.Manager.Import")
	{
		// Eine Datei, vier moegliche Bedeutungen - was es ist, erkennt
		// Transfer::classify am Inhalt, nicht an der Endung. Das Fenster
		// bleibt dabei offen: wenn die Datei angekommen ist, schaltet
		// pollImport() auf ihre Art um und zeigt sie in der Liste.
		if(!Transfer::beginImport()) engine.showToast(Engine::TOAST_ERROR, "$TR_ERROR_CLICK_AGAIN");
	}
	else if(name == "Menu.ManagerPane.Manager.Export")
	{
		GUI_ListBox* p_list = static_cast<GUI_ListBox*>(gui["Menu.ManagerPane.Manager.Items"]);
		if(p_list->getSelection() == -1)
		{
			// Von dieser Sorte ist noch nichts da. Der Knopf ist dann zwar
			// abgeschaltet, aber die Eingabetaste der Liste kommt auch hier
			// an, und ohne die Meldung taete sie schlicht nichts.
			engine.showToast(Engine::TOAST_ERROR, "$TR_NOTHING_TO_EXPORT");
			return;
		}

		// Nur vormerken - der Dialog laeuft eine Runde spaeter in
		// pollExport(), aus demselben Grund wie beim Import.
		pendingExportKind = currentManagerKind();
		pendingExportName = p_list->getSelectedItemText();
		pendingExport = true;

		gui["Menu.ManagerPane"]->hide();
		gui["Menu"]->focus();
	}
	else if(name == "Menu.ManagerPane.Manager.Delete")
	{
		GUI_ListBox* p_list = static_cast<GUI_ListBox*>(gui["Menu.ManagerPane.Manager.Items"]);
		if(p_list->getSelection() == -1) return;

		// Das Einzige im Manager, was sich nicht rueckgaengig machen laesst -
		// also erst fragen. Was geloescht werden soll, steht ab hier fest und
		// wird nicht spaeter noch einmal aus der Liste geholt.
		pendingDeleteKind = currentManagerKind();
		pendingDeleteName = p_list->getSelectedItemText();

		gui["Menu.ConfirmPane"]->show();
		gui["Menu.ConfirmPane.Confirm"]->focus();
	}
	else if(name == "Menu.ConfirmPane.Confirm.Yes")
	{
		gui["Menu.ConfirmPane"]->hide();
		gui["Menu.ManagerPane.Manager"]->focus();

		std::string errorId;
		if(Transfer::remove(static_cast<Transfer::Kind>(pendingDeleteKind), pendingDeleteName, errorId))
		{
			engine.showToast(Engine::TOAST_OK, localizeString("$TR_DELETED") + " \"" + pendingDeleteName + "\"");
		}
		else engine.showToast(Engine::TOAST_ERROR, errorId.empty() ? "$TR_ERROR_FAILED" : errorId);

		pendingDeleteName = "";
		refreshManagerList();
	}
	else if(name == "Menu.ConfirmPane.Confirm.No")
	{
		gui["Menu.ConfirmPane"]->hide();
		gui["Menu.ManagerPane.Manager"]->focus();
		pendingDeleteName = "";
	}
	else if(name == "Menu.Quit")
	{
		SDL_Event event;
		event.type = SDL_QUIT;
		SDL_PushEvent(&event);
	}
	else if(name == "Menu.Website")
	{
		// Der unsichtbare Knopf ueber der Adresse im Hintergrundbild.
#ifdef __EMSCRIPTEN__
		// _blank, damit das Spiel in seinem Tab weiterlaeuft. Der Klick liegt
		// nur einen Frame zurueck, also gilt die Seite dem Browser noch als
		// kuerzlich bedient und der Popup-Blocker laesst das Fenster durch.
		EM_ASM({ window.open(UTF8ToString($0), "_blank"); }, "https://www.david-scherfgen.de/");
#else
		// Wie beim Spendenknopf: unter Windows liegt die Adresse in einer
		// .url-Datei neben der Anwendung, die zugleich im Startmenue steht.
		ShellExecuteA(0, "open", "Scherfgen-Software Website.url", 0, 0, SW_SHOWMAXIMIZED);
#endif
	}
	else if(name == "Menu.Donate")
	{
		gui["Menu.DonatePane.Donate"]->focus();
	}
	else if(name == "Menu.CrtPane.Crt.TryIt" ||
			name == "Menu.CrtPane.Crt.NoThanks")
	{
		if(name == "Menu.CrtPane.Crt.TryIt")
		{
			engine.setUpscaleFilter(Engine::UF_CRT);
			engine.saveConfig();
		}

		// So oder so gefragt. Die Merkdatei haelt nur fest, dass es passiert
		// ist; ihr Inhalt wird nirgends gelesen.
		FileSystem& fs = FileSystem::inst();
		fs.writeStringToFile("1", fs.getAppHomeDirectory() + ".crt_offered");

		gui["Menu.CrtPane"]->hide();
		gui["Menu"]->focus();
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
#ifdef __EMSCRIPTEN__
			// Neben der Anwendung liegt hier kein Verzeichnis, in dem eine
			// .url-Verknuepfung stehen koennte - der Browser bekommt die
			// Adresse also direkt. Sie muss mit der uebereinstimmen, die in
			// "Donate (<Sprache>).url" steht: das ist die Datei, die die
			// Windows-Fassung unten oeffnet.
			const std::string url = engine.getLanguage() == "de"
				? "https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=UUFVK97YL6ZHY"
				: "https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=FMADXSNPDGRCW";
			// _blank, damit das Spiel in seinem Tab weiterlaeuft. Der Klick
			// liegt nur einen Frame zurueck, also gilt die Seite dem Browser
			// noch als "kuerzlich bedient" und der Popup-Blocker laesst das
			// Fenster durch.
			EM_ASM({ window.open(UTF8ToString($0), "_blank"); }, url.c_str());
#else
			const std::string urlPath(std::string("Donate (") + engine.getLanguage() + ").url");
			ShellExecuteA(0, "open", urlPath.c_str(), 0, 0, SW_SHOWMAXIMIZED);
#endif
		}
	}
}

void GS_Menu::pollImport()
{
	std::string path, untrustedName;
	const int status = Transfer::pollImport(path, untrustedName);
	if(status == Transfer::STATUS_BUSY) return;
	if(status == Transfer::STATUS_CANCELLED) { Transfer::finishImport(); return; }

	if(status != Transfer::STATUS_OK)
	{
		Transfer::finishImport();
		engine.showToast(Engine::TOAST_ERROR, status == Transfer::STATUS_TOO_BIG ? "$TR_ERROR_TOO_BIG"
										   : status == Transfer::STATUS_UNKNOWN ? "$TR_ERROR_UNKNOWN"
										   : "$TR_ERROR_FAILED");
		return;
	}

	const Transfer::Kind kind = Transfer::classify(path);
	if(kind == Transfer::KIND_NONE)
	{
		Transfer::finishImport();
		engine.showToast(Engine::TOAST_ERROR, "$TR_ERROR_UNKNOWN");
		return;
	}

	std::string errorId;
	bool replaced = false;
	const std::string name(Transfer::install(kind, path, untrustedName, errorId, &replaced));
	Transfer::finishImport();

	if(name.empty())
	{
		engine.showToast(Engine::TOAST_ERROR, errorId.empty() ? "$TR_ERROR_FAILED" : errorId);
		return;
	}

	// Steht der Manager noch offen, dann sieht der Spieler gerade die Liste,
	// in die die Datei gewandert ist - also auf ihre Art umschalten, neu
	// einlesen und den neuen Eintrag markieren. Das ist der eigentliche
	// Gewinn daran, Import und Export in einem Fenster zu haben: vorher
	// blieb es bei der Meldung, und wo die Datei gelandet war, bekam man nie
	// zu sehen.
	if(gui["Menu.ManagerPane"]->isVisible())
	{
		setManagerKind(kind);
		refreshManagerList();

		GUI_ListBox* p_list = static_cast<GUI_ListBox*>(gui["Menu.ManagerPane.Manager.Items"]);
		const int where = p_list->findItem(name);
		if(where != -1) p_list->setSelection(where);
		updateManagerButtons();
	}

	// Wurde etwas ersetzt, dann ist genau das die Nachricht: die Datei, die
	// vorher unter diesem Namen lag, ist weg. Den Namen musste der Spieler
	// dafuer schon kennen - er hat ihn ja vergeben -, also entfaellt hier der
	// Hinweis, wo man ihn eintraegt.
	if(replaced)
	{
		engine.showToast(Engine::TOAST_OK, localizeString("$TR_REPLACED") + " \"" + name + "\"");
		return;
	}

	// Bei Level, Musik und Skin ist der vergebene Name das, was der Spieler
	// gleich irgendwo eintragen muss - also mit ausgeben. Er kommt aus
	// sanitizeFilenameStem und besteht nur aus [A-Za-z0-9_-] plus Endung,
	// kann also keine Lokalisierungsmarke enthalten.
	switch(kind)
	{
	case Transfer::KIND_CAMPAIGN:
		engine.showToast(Engine::TOAST_OK, "$TR_IMPORTED_CAMPAIGN");
		break;
	case Transfer::KIND_MUSIC:
		engine.showToast(Engine::TOAST_OK, localizeString("$TR_IMPORTED_MUSIC") + " \"" + name + "\"");
		break;
	case Transfer::KIND_SKIN:
		// Ohne ".zip" und ohne den Punkt: so, wie der Name in ein Skin-Feld
		// geschrieben wird. setFilenameExtension laesst den Punkt stehen.
		engine.showToast(Engine::TOAST_OK, localizeString("$TR_IMPORTED_SKIN") + " \"" +
										   name.substr(0, name.find_last_of('.')) + "\"");
		break;
	default:
		engine.showToast(Engine::TOAST_OK, localizeString("$TR_IMPORTED_LEVEL") + " \"" + name + "\"");
		break;
	}
}

void GS_Menu::pollExport()
{
	if(!pendingExport) return;
	pendingExport = false;

	std::string errorId;
	const bool ok = Transfer::doExport(static_cast<Transfer::Kind>(pendingExportKind),
									   pendingExportName, errorId);
	if(ok) engine.showToast(Engine::TOAST_OK, "$TR_EXPORTED");
	else if(!errorId.empty()) engine.showToast(Engine::TOAST_ERROR, errorId);
}

void GS_Menu::openManager()
{
	setManagerKind(Transfer::KIND_LEVEL);
	refreshManagerList();
	gui["Menu.ManagerPane"]->show();
	gui["Menu.ManagerPane.Manager"]->focus();
}

int GS_Menu::currentManagerKind() const
{
	if(static_cast<GUI_RadioButton*>(gui["Menu.ManagerPane.Manager.KindCampaign"])->isChecked()) return Transfer::KIND_CAMPAIGN;
	if(static_cast<GUI_RadioButton*>(gui["Menu.ManagerPane.Manager.KindMusic"])->isChecked())    return Transfer::KIND_MUSIC;
	if(static_cast<GUI_RadioButton*>(gui["Menu.ManagerPane.Manager.KindSkin"])->isChecked())     return Transfer::KIND_SKIN;
	return Transfer::KIND_LEVEL;
}

void GS_Menu::setManagerKind(int kind)
{
	// check() und nicht setChecked(): der Wechsel soll wirken, als haette ihn
	// jemand angeklickt. Die Liste liest der Aufrufer danach ohnehin neu ein.
	const char* p_name = "Menu.ManagerPane.Manager.KindLevel";
	switch(kind)
	{
	case Transfer::KIND_CAMPAIGN: p_name = "Menu.ManagerPane.Manager.KindCampaign"; break;
	case Transfer::KIND_MUSIC:    p_name = "Menu.ManagerPane.Manager.KindMusic";    break;
	case Transfer::KIND_SKIN:     p_name = "Menu.ManagerPane.Manager.KindSkin";     break;
	default: break;
	}
	static_cast<GUI_RadioButton*>(gui[p_name])->check();
}

void GS_Menu::refreshManagerList()
{
	GUI_ListBox* p_list = static_cast<GUI_ListBox*>(gui["Menu.ManagerPane.Manager.Items"]);
	p_list->clear();

	const std::vector<std::string> items(Transfer::list(static_cast<Transfer::Kind>(currentManagerKind())));
	for(uint i = 0; i < items.size(); i++)
	{
		p_list->addItem(GUI_ListBox::ListItem(items[i], 0));
	}
	p_list->setSelection(items.empty() ? -1 : 0);

	// setSelection() meldet sich nur, wenn sich die Nummer wirklich aendert -
	// bei einer Liste, die vorher wie nachher auf 0 steht, also nicht. Der
	// Inhalt kann trotzdem ein anderer sein, deshalb hier von Hand.
	updateManagerButtons();
}

void GS_Menu::updateManagerButtons()
{
	GUI_ListBox* p_list = static_cast<GUI_ListBox*>(gui["Menu.ManagerPane.Manager.Items"]);
	const bool haveSelection = p_list->getSelection() != -1;

	GUI_Button* p_export = static_cast<GUI_Button*>(gui["Menu.ManagerPane.Manager.Export"]);
	if(haveSelection) p_export->activate();
	else              p_export->deactivate();

	// Die sieben mitgelieferten Dateien stehen in der Liste - man kann sie
	// ausgeben und weitergeben -, aber loeschen laesst sich keine davon: der
	// Skin, den ein Level nennt, waere sonst weg, und das Spiel liefert
	// nichts nach.
	const bool canDelete = haveSelection &&
						   !Transfer::isBuiltIn(static_cast<Transfer::Kind>(currentManagerKind()),
												p_list->getSelectedItemText());

	GUI_Button* p_delete = static_cast<GUI_Button*>(gui["Menu.ManagerPane.Manager.Delete"]);
	if(canDelete) p_delete->activate();
	else          p_delete->deactivate();
}
