#include "pch.h"
#include "gs_menu.h"
#include "u_crt.h"
#include "gui.h"
#include "gui_all.h"
#include "cf_all.h"
#include "texture.h"
#include "options.h"
#include "help.h"
#include "filesystem.h"
#include "transfer.h"
#include "progressdb.h"
#ifdef _WIN32
#include <shellapi.h>
#endif
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

extern const char* p_localVersion;

namespace
{
	// demo1.dat holds raw key numbers - SDL 1.2's, the ones the recording
	// came from. Emscripten's SDL counts differently: SDLK_LEFT is 1104
	// there and not 276. The table maps the recorded number onto the
	// constant *this* build means.
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
	confirmMode = CONFIRM_NONE;
	pendingDeleteKind = 0;
	pendingImportKind = 0;
}

GS_Menu::~GS_Menu()
{
}

void GS_Menu::onRender()
{
	// render the colour gradient
	glBegin(GL_QUADS);
	glColor3d(0.5, 0.5, 1.0);
	glVertex2i(0, 0);
	glVertex2i(640, 0);
	glColor3d(0.75, 0.7, 1.0);
	glVertex2i(640, 480);
	glVertex2i(0, 480);
	glEnd();

	// render the clouds
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

	// render the title level
	glPushMatrix();
	glTranslated(0.0, 65.0, 0.0);
	p_titleLevel->render();
	glPopMatrix();

	// render the background image
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
	// The file dialogs run here, not in the click handler: under Windows they
	// are modal and would otherwise start a second message loop inside the
	// GUI.
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

	// Escape quits the game. It must stand *before* the demo playback: just
	// below, wasKeyPressed() reads the recording and no longer the keyboard.
	// Not while the donation question is open, and not when Options or Help
	// have just used the key themselves - they unregister it with
	// consumeKeyPress(), because GUI::update() runs before onUpdate().
	if(engine.wasKeyPressed(SDLK_ESCAPE) &&
	   !gui["Menu.DonatePane"]->isVisible() &&
	   !gui["Menu.CrtPane"]->isVisible())
	{
		// The Manager and its confirmation take the key for themselves, from
		// the top down.
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

	// From here on the keyboard belongs to the demo. SDLK_LAST, not 512:
	// Emscripten's SDL counts up to 1536 and the arrow keys sit beyond 512
	// there - uncleared, one of them would stay pressed for ever.
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
	// load the images
	p_clouds = Manager<Texture>::inst().request("clouds.png");
	p_background = Manager<Texture>::inst().request("menu.png");

	// build the menu
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
	static_cast<GUI_RadioButton*>(gui["Menu.ManagerPane.Manager.KindProgress"])->connectChanged(this, &GS_Menu::handleClick);

	// Export and Delete depend on the selection, hence the list has to say
	// when it changes.
	static_cast<GUI_ListBox*>(gui["Menu.ManagerPane.Manager.Items"])->connectChanged(this, &GS_Menu::handleClick);

	static_cast<GUI_Button*>(gui["Menu.ConfirmPane.Confirm.Yes"])->connectClicked(this, &GS_Menu::handleClick);
	static_cast<GUI_Button*>(gui["Menu.ConfirmPane.Confirm.Merge"])->connectClicked(this, &GS_Menu::handleClick);
	static_cast<GUI_Button*>(gui["Menu.ConfirmPane.Confirm.No"])->connectClicked(this, &GS_Menu::handleClick);

	FileSystem& fs = FileSystem::inst();

	// Offer the CRT filter once: on a first start after the installation and
	// equally after an update, because the marker file exists nowhere before
	// 1.2.0. Only where the machine can show the filter and it is not
	// already switched on.
	const std::string crtOfferedPath(fs.getAppHomeDirectory() + ".crt_offered");
	const bool offerCrt = engine.getCrt().isAvailable() &&
						  engine.getUpscaler() != &engine.getCrt() &&
						  !fs.fileExists(crtOfferedPath);
	if(offerCrt) gui["Menu.CrtPane.Crt"]->focus();

	// When did the donation question last come up?
	const std::string lastAskedForDonationStr = fs.fileExists(fs.getAppHomeDirectory() + ".donation_asked") ? fs.readStringFromFile(fs.getAppHomeDirectory() + ".donation_asked") : "";
	if(lastAskedForDonationStr != "disable")
	{
		const uint lastAskedForDonation = static_cast<uint>(atoi(lastAskedForDonationStr.c_str()));
		const uint timePlayed = engine.getTimePlayed();

		// Both numbers are unsigned: if .donation_asked holds a larger one
		// than .time_played, the difference overflows and the donation window
		// would come up at every start.
		// Not both at once - the donation question comes next time.
		if(!offerCrt &&
		   timePlayed >= lastAskedForDonation &&
		   timePlayed - lastAskedForDonation >= 60 * (60 * 60 * 3))
		{
			gui["Menu.DonatePane.Donate"]->focus();
		}
	}

	p_options = new Options(0);
	p_help = new Help(0);

	// load the keyboard data for the demo
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
	// release the images
	p_clouds->release();
	p_clouds = 0;
	p_background->release();
	p_background = 0;

	if(p_titleLevel) delete p_titleLevel;
	p_titleLevel = 0;
	levelSaved = false;

	// Give up a file dialog that is still open, or it keeps the channel
	// occupied until the browser discards it itself after five minutes.
	Transfer::abandonImport();

	// delete the menu
	delete gui["Menu"];
	delete p_options;
	delete p_help;
}

void GS_Menu::onGetFocus()
{
	engine.playMusic("menu.ogg", 0.0, true);

	// load the title level
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
	// save and delete the title level
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
			name == "Menu.ManagerPane.Manager.KindProgress" ||
			name == "Menu.ManagerPane.Manager.Refresh")
	{
		// Re-read on a change of kind and on request: while the pane is open
		// the directory can have changed.
		refreshManagerList();
	}
	else if(name == "Menu.ManagerPane.Manager.Items")
	{
		// The selection has changed - Export and Delete follow it.
		updateManagerButtons();
	}
	else if(name == "Menu.ManagerPane.Manager.Close")
	{
		gui["Menu.ManagerPane"]->hide();
		gui["Menu"]->focus();
	}
	else if(name == "Menu.ManagerPane.Manager.Import")
	{
		// One file, four possible meanings - Transfer::classify works out
		// which from the content. The pane stays open meanwhile.
		if(!Transfer::beginImport()) engine.showToast(Engine::TOAST_ERROR, "$TR_ERROR_CLICK_AGAIN");
	}
	else if(name == "Menu.ManagerPane.Manager.Export")
	{
		GUI_ListBox* p_list = static_cast<GUI_ListBox*>(gui["Menu.ManagerPane.Manager.Items"]);
		if(p_list->getSelection() == -1)
		{
			// Nothing of this kind is there yet. The button is deactivated,
			// but the list's Return key arrives here as well.
			engine.showToast(Engine::TOAST_ERROR, "$TR_NOTHING_TO_EXPORT");
			return;
		}

		// Only note it down - the dialog runs a round later in pollExport(),
		// for the same reason as the import. The pane stays open: the Manager
		// is a place you keep working in.
		pendingExportKind = currentManagerKind();
		pendingExportName = p_list->getSelectedItemText();
		pendingExport = true;
	}
	else if(name == "Menu.ManagerPane.Manager.Delete")
	{
		GUI_ListBox* p_list = static_cast<GUI_ListBox*>(gui["Menu.ManagerPane.Manager.Items"]);
		if(p_list->getSelection() == -1) return;

		// The one thing in the Manager that cannot be undone - hence the
		// question first. What gets deleted is settled from here on.
		confirmMode = CONFIRM_DELETE;
		pendingDeleteKind = currentManagerKind();
		pendingDeleteName = p_list->getSelectedItemText();

		// Deleting the progress is not the same as deleting a file that can
		// be imported again, and the question says so.
		askConfirmation(pendingDeleteKind == Transfer::KIND_PROGRESS
						? "$TR_CONFIRM_DELETE_PROGRESS" : "$TR_CONFIRM_DELETE",
						"$YES", "$NO", false);
	}
	else if(name == "Menu.ConfirmPane.Confirm.Yes")
	{
		const int mode = confirmMode;
		const int kind = pendingImportKind;
		const std::string path(pendingImportPath), untrustedName(pendingImportName);
		const std::string deleteName(pendingDeleteName);
		closeConfirmation();

		if(mode == CONFIRM_OVERWRITE)
		{
			completeImport(kind, path, untrustedName);
		}
		else if(mode == CONFIRM_DELETE)
		{
			std::string errorId;
			if(Transfer::remove(static_cast<Transfer::Kind>(pendingDeleteKind), deleteName, errorId))
			{
				engine.showToast(Engine::TOAST_OK, localizeString("$TR_DELETED") + " \"" + deleteName + "\"");
			}
			else engine.showToast(Engine::TOAST_ERROR, errorId.empty() ? "$TR_ERROR_FAILED" : errorId);

			refreshManagerList();
		}
	}
	else if(name == "Menu.ConfirmPane.Confirm.Merge")
	{
		const std::string path(pendingImportPath);
		const bool merging = (confirmMode == CONFIRM_OVERWRITE);
		closeConfirmation();
		if(merging) mergeImport(path);
	}
	else if(name == "Menu.ConfirmPane.Confirm.No")
	{
		// An import that was refused still has to be let go of: in the
		// browser the bytes lie in a staging file, and nothing else deletes
		// it once pollImport() has handed it over.
		const bool wasImport = (confirmMode == CONFIRM_OVERWRITE);
		closeConfirmation();
		if(wasImport) Transfer::finishImport();
	}
	else if(name == "Menu.Quit")
	{
		SDL_Event event;
		event.type = SDL_QUIT;
		SDL_PushEvent(&event);
	}
	else if(name == "Menu.Website")
	{
		// The invisible button over the address in the background image.
#if defined(__EMSCRIPTEN__)
		// _blank keeps the game running in its own tab. The click is only one
		// frame back, hence the popup blocker lets the window through.
		EM_ASM({ window.open(UTF8ToString($0), "_blank"); }, "https://www.david-scherfgen.de/");
#elif defined(_WIN32)
		// As with the donate button: under Windows the address lives in a
		// .url file beside the application, and that file is in the Start
		// menu too.
		ShellExecuteA(0, "open", "Scherfgen-Software Website.url", 0, 0, SW_SHOWMAXIMIZED);
#else
		openURL("https://www.david-scherfgen.de/");
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
			engine.setUpscaler(&engine.getCrt());
			engine.saveConfig();
		}

		// Asked either way. The marker file only records that it happened;
		// its contents are read nowhere.
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
			// remember that a donation has been asked for
			std::ostringstream temp;
			temp << engine.getTimePlayed();
			fs.writeStringToFile(temp.str(), fs.getAppHomeDirectory() + ".donation_asked");
		}

		if(name == "Menu.DonatePane.Donate.Donate")
		{
			gui["Menu.DonatePane"]->hide();
#ifdef _WIN32
			const std::string urlPath(std::string("Donate (") + engine.getLanguage() + ").url");
			ShellExecuteA(0, "open", urlPath.c_str(), 0, 0, SW_SHOWMAXIMIZED);
#else
			// There is no directory for a .url shortcut beside the
			// application here; the address must match the one in
			// "Donate (<language>).url".
			const std::string url = engine.getLanguage() == "de"
				? "https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=UUFVK97YL6ZHY"
				: "https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=FMADXSNPDGRCW";
#ifdef __EMSCRIPTEN__
			// _blank keeps the game running in its own tab. The click is only
			// one frame back, hence the popup blocker lets it through.
			EM_ASM({ window.open(UTF8ToString($0), "_blank"); }, url.c_str());
#else
			openURL(url);
#endif
#endif
		}
	}
}

void GS_Menu::pollImport()
{
	// Not while a question is on the screen. pollImport() latches its answer
	// once, so asking for it here would take the import out of Transfer's
	// hands and put its question over the one the player is still looking at.
	// Leaving it in the pipe costs a tick or two and nothing else.
	if(confirmMode != CONFIRM_NONE) return;

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

	// Something of the player's would be gone. Ask first - and hold the whole
	// import back until the answer, finishImport() included, since in the
	// browser that deletes the staging file the bytes are in.
	if(Transfer::wouldReplace(kind, untrustedName))
	{
		confirmMode = CONFIRM_OVERWRITE;
		pendingImportKind = kind;
		pendingImportPath = path;
		pendingImportName = untrustedName;

		// Only a progress database can be taken into the one already there;
		// for everything else there is nothing to combine.
		const bool progress = (kind == Transfer::KIND_PROGRESS);
		// Named after what they do, not after yes and no: neither is an answer
		// to a question that offers replacing and merging.
		askConfirmation(progress ? localizeString("$TR_CONFIRM_MERGE")
								 : localizeString("$TR_CONFIRM_OVERWRITE") + " \"" +
								   Transfer::targetName(kind, untrustedName) + "\"",
						"$TR_REPLACE_DO", "$CANCEL", progress);
		return;
	}

	completeImport(kind, path, untrustedName);
}

void GS_Menu::completeImport(int kind,
							 const std::string& path,
							 const std::string& untrustedName)
{
	std::string errorId;
	bool replaced = false;
	const std::string name(Transfer::install(static_cast<Transfer::Kind>(kind), path,
											 untrustedName, errorId, &replaced));
	Transfer::finishImport();

	if(name.empty())
	{
		engine.showToast(Engine::TOAST_ERROR, errorId.empty() ? "$TR_ERROR_FAILED" : errorId);
		return;
	}

	// If the Manager is still open, the player is looking at the very list
	// the file has moved into - hence switch to its kind, re-read the list
	// and select the new entry.
	if(gui["Menu.ManagerPane"]->isVisible())
	{
		setManagerKind(kind);
		refreshManagerList();

		GUI_ListBox* p_list = static_cast<GUI_ListBox*>(gui["Menu.ManagerPane.Manager.Items"]);
		const int where = p_list->findItem(name);
		if(where != -1) p_list->setSelection(where);
		updateManagerButtons();
	}

	// If something got replaced, that is exactly the message: the file that
	// lay under this name before is gone.
	if(replaced)
	{
		engine.showToast(Engine::TOAST_OK, localizeString("$TR_REPLACED") + " \"" + name + "\"");
		return;
	}

	// For a level, music and a skin the assigned name is what the player has
	// to enter somewhere next - hence report it as well. It consists only of
	// [A-Za-z0-9_-] plus the extension and can therefore hold no
	// localization marker.
	switch(kind)
	{
	case Transfer::KIND_CAMPAIGN:
		engine.showToast(Engine::TOAST_OK, "$TR_IMPORTED_CAMPAIGN");
		break;
	case Transfer::KIND_MUSIC:
		engine.showToast(Engine::TOAST_OK, localizeString("$TR_IMPORTED_MUSIC") + " \"" + name + "\"");
		break;
	case Transfer::KIND_SKIN:
		// Without ".zip" and without the dot: the way the name goes into a
		// skin slot. setFilenameExtension leaves the dot in place.
		engine.showToast(Engine::TOAST_OK, localizeString("$TR_IMPORTED_SKIN") + " \"" +
										   name.substr(0, name.find_last_of('.')) + "\"");
		break;
	case Transfer::KIND_PROGRESS:
		// No name with this one: there is only ever the one file, and saying
		// "progress.zip" would tell the player nothing they can use.
		engine.showToast(Engine::TOAST_OK, "$TR_IMPORTED_PROGRESS");
		break;
	default:
		engine.showToast(Engine::TOAST_OK, localizeString("$TR_IMPORTED_LEVEL") + " \"" + name + "\"");
		break;
	}
}

void GS_Menu::mergeImport(const std::string& path)
{
	// classify() only asked whether the archive lists a progress.xml, which
	// the table of contents answers without opening it. Reading it can still
	// fail, and an empty union would then be reported as a merge that worked.
	if(!ProgressDB::canRead(path))
	{
		Transfer::finishImport();
		engine.showToast(Engine::TOAST_ERROR, "$TR_ERROR_BROKEN");
		return;
	}

	// The union, and no code of its own for it: read the imported database
	// and mark everything in it as solved. markSolved() reads the player's
	// own file first, so what comes out holds both.
	ProgressDB& db = ProgressDB::inst();
	const ProgressDB::Progress other(db.query(path));

	std::vector<std::pair<std::string, uint> > solved;
	for(ProgressDB::Progress::const_iterator i = other.begin(); i != other.end(); ++i)
	{
		for(std::set<uint>::const_iterator j = i->second.begin(); j != i->second.end(); ++j)
		{
			solved.push_back(std::make_pair(i->first, *j));
		}
	}

	const bool ok = db.markSolved(solved);

	// Only now: in the browser this deletes the staging file the database was
	// read out of a moment ago.
	Transfer::finishImport();

	if(ok) engine.showToast(Engine::TOAST_OK, "$TR_MERGED");
	else   engine.showToast(Engine::TOAST_ERROR, "$TR_ERROR_FAILED");

	if(gui["Menu.ManagerPane"]->isVisible())
	{
		setManagerKind(Transfer::KIND_PROGRESS);
		refreshManagerList();
	}
}

void GS_Menu::askConfirmation(const std::string& text,
							  const std::string& yesTitle,
							  const std::string& noTitle,
							  bool offerMerge)
{
	static_cast<GUI_StaticText*>(gui["Menu.ConfirmPane.Confirm.Text"])->setText(text);
	static_cast<GUI_Button*>(gui["Menu.ConfirmPane.Confirm.Yes"])->setTitle(yesTitle);
	static_cast<GUI_Button*>(gui["Menu.ConfirmPane.Confirm.No"])->setTitle(noTitle);

	GUI_Element* p_merge = gui["Menu.ConfirmPane.Confirm.Merge"];
	if(offerMerge) p_merge->show();
	else           p_merge->hide();

	gui["Menu.ConfirmPane"]->show();
	gui["Menu.ConfirmPane.Confirm"]->focus();
}

void GS_Menu::closeConfirmation()
{
	gui["Menu.ConfirmPane"]->hide();

	// Only back to the Manager if it is still open. focus() shows what it
	// focuses and every parent of it, so on a file dialog the player left the
	// Manager during - which is what the asynchronous ones allow - this would
	// open the pane again by itself.
	if(gui["Menu.ManagerPane"]->isVisible()) gui["Menu.ManagerPane.Manager"]->focus();
	else                                     gui["Menu"]->focus();

	confirmMode = CONFIRM_NONE;
	pendingDeleteName = "";
	pendingImportPath = "";
	pendingImportName = "";
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
	if(static_cast<GUI_RadioButton*>(gui["Menu.ManagerPane.Manager.KindProgress"])->isChecked()) return Transfer::KIND_PROGRESS;
	return Transfer::KIND_LEVEL;
}

void GS_Menu::setManagerKind(int kind)
{
	// check() and not setChecked(): the change should act as if somebody had
	// clicked it. The caller re-reads the list afterwards anyway.
	const char* p_name = "Menu.ManagerPane.Manager.KindLevel";
	switch(kind)
	{
	case Transfer::KIND_CAMPAIGN: p_name = "Menu.ManagerPane.Manager.KindCampaign"; break;
	case Transfer::KIND_MUSIC:    p_name = "Menu.ManagerPane.Manager.KindMusic";    break;
	case Transfer::KIND_SKIN:     p_name = "Menu.ManagerPane.Manager.KindSkin";     break;
	case Transfer::KIND_PROGRESS: p_name = "Menu.ManagerPane.Manager.KindProgress"; break;
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

	// setSelection() reports only when the index really changes. The content
	// can be a different one regardless, hence by hand here.
	updateManagerButtons();
}

void GS_Menu::updateManagerButtons()
{
	GUI_ListBox* p_list = static_cast<GUI_ListBox*>(gui["Menu.ManagerPane.Manager.Items"]);
	const bool haveSelection = p_list->getSelection() != -1;

	GUI_Button* p_export = static_cast<GUI_Button*>(gui["Menu.ManagerPane.Manager.Export"]);
	if(haveSelection) p_export->activate();
	else              p_export->deactivate();

	// What the game ships is listed - it can be exported - but it cannot be
	// deleted: the skin a level names would otherwise be gone, and the game
	// delivers no replacement. With the two example levels there is nothing
	// to delete until the player has saved one of them themselves.
	const bool canDelete = haveSelection &&
						   Transfer::isRemovable(static_cast<Transfer::Kind>(currentManagerKind()),
												 p_list->getSelectedItemText());

	GUI_Button* p_delete = static_cast<GUI_Button*>(gui["Menu.ManagerPane.Manager.Delete"]);
	if(canDelete) p_delete->activate();
	else          p_delete->deactivate();
}
