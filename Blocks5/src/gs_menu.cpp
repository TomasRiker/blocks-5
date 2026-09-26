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
	// demo1.dat holds SDL 1.2's raw key numbers, and Emscripten's SDL counts
	// differently (SDLK_LEFT is 1104 there, not 276). The table maps each
	// recorded number onto the constant *this* build means.
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
	p_options = 0;
	p_help = 0;
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
	Renderer& renderer = Renderer::inst();
	const Vec2f screen[4] = {Vec2f(0.0f, 0.0f), Vec2f(640.0f, 0.0f), Vec2f(640.0f, 480.0f), Vec2f(0.0f, 480.0f)};

	// render the colour gradient
	{
		const Vec4f top(0.5f, 0.5f, 1.0f, 1.0f), bottom(0.75f, 0.7f, 1.0f, 1.0f);
		const Vec4f colors[4] = {top, top, bottom, bottom};
		renderer.quad(screen, colors);
	}

	// render the clouds: each layer scrolls on top of the picture's own
	// scale, a texture matrix of its own
	const TextureRef clouds = p_clouds->ref();
	for(int i = 2; i >= 0; i--)
	{
		float s[] = {1.0f, 0.5f, 0.25f};
		const float fi = static_cast<float>(i);
		// The offset (100 * i + 0.05 * time) and the wobble's phase (0.02 * s
		// of the offset) are both linear in the clock. The wobble, bounded by
		// its sine, is added after the reduction and the sum wrapped again.
		float x = scrollOffset(time, 0.05f, 100.0f * fi, static_cast<float>(p_clouds->getSize().x));
		x += 2.0f * sinf(clockPhase(time, 0.001f * s[i], 2.0f * s[i] * fi + fi));
		x = wrapTextureOffset(x, p_clouds->getSize().x);

		Mat4 scroll = Mat4::scaling(clouds.texelScale.x, clouds.texelScale.y, 1.0f);
		scroll.scale(s[i], s[i], s[i]);
		scroll.translate(-x / s[i], 0.0f, 0.0f);
		scroll.rotate(15.0f + 5.0f * i, 0.0f, 0.0f, 1.0f);
		const float c = 1.0f - 0.05f * i;
		const float a = 0.4f - 0.05f * i;
		renderer.scrolledQuad(clouds, scroll, screen, screen, Vec4f(c, c, c, a));
	}

	// render the title level
	renderer.push();
	renderer.translate(0.0f, 65.0f);
	p_titleLevel->render();
	renderer.pop();

	// render the background image
	renderer.setTexture(p_background->ref());
	renderer.quad(renderer.state(), screen, screen, Vec4f(1.0f, 1.0f, 1.0f, 1.0f));
}

void GS_Menu::onUpdate()
{
	// The file dialogs run here, not in the click handler: under Windows they
	// are modal and would start a second message loop inside the GUI.
	pollImport();
	pollExport();


#ifdef __EMSCRIPTEN__
	Uint8* p_keyStates = SDL_GetKeyboardState(0);
#else
	Uint8* p_keyStates = SDL_GetKeyState(0);
#endif
	// Ctrl+Shift+F2 is the plain credits and Ctrl+Shift+F3 the ending, named
	// outright so that what the author sees does not depend on the save file;
	// the Credits entry gets what the player has earned. Function keys,
	// because pre.js keeps F1 to F24 from the browser while every
	// Ctrl+Shift+<letter> is some browser's shortcut. The key is
	// wasKeyPressed()'s edge: a level test would need it held past a rendered
	// frame, a fifth of a second under llvmpipe.
	const bool ctrlShiftHeld =
		(p_keyStates[SDLK_LCTRL] || p_keyStates[SDLK_RCTRL]) &&
		(p_keyStates[SDLK_LSHIFT] || p_keyStates[SDLK_RSHIFT]);
	if(ctrlShiftHeld &&
	   (engine.wasKeyPressed(SDLK_F2) || engine.wasKeyPressed(SDLK_F3)))
	{
		ParameterBlock context;
		context.set("full", engine.wasKeyPressed(SDLK_F3));
		engine.setGameState("GS_Credits", context);
		engine.crossfade(new CF_Star, 0.85f);
	}
	// Ctrl+Shift+F4 turns the donation question off for good. Alt+F4, the one
	// other reader of this key, wants Alt instead.
	else if(ctrlShiftHeld && engine.wasKeyPressed(SDLK_F4))
	{
		FileSystem& fs = FileSystem::inst();
		fs.writeStringToFile("disable", fs.getAppHomeDirectory() + ".donation_asked");
	}

	// Escape quits the game. It must come before the demo playback, after
	// which wasKeyPressed() reads the recording rather than the keyboard. Not
	// while the donation or CRT question is open, nor when Options or Help
	// have just used the key: they consumeKeyPress() it, since GUI::update()
	// runs before onUpdate().
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

	// From here on the keyboard belongs to the demo. All of SDLK_LAST:
	// Emscripten's SDL counts up to 1536 with the arrow keys beyond 512, and
	// one left uncleared would stay pressed for ever.
	for(int i = 0; i < SDLK_LAST; i++)
	{
		engine.setKeyData(static_cast<SDLKey>(i), 0);
	}

	// The recording replays half a second in: before that there is nothing
	// to find, and time - 500 would wrap.
	std::unordered_map<uint, std::list<uint> >::const_iterator i =
		time >= 500 ? keyData.find(time - 500) : keyData.end();
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
	p_clouds = Manager<Texture>::inst().request("clouds.png", Texture::WM_REPEAT);
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
	static_cast<GUI_Button*>(gui["Menu.Credits"])->connectClicked(this, &GS_Menu::handleClick);
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

	// Offer the CRT filter once: on the first start of a fresh installation,
	// or of an update from before 1.2.0, which never wrote the marker. Not to
	// somebody who has it on already.
	const std::string crtOfferedPath(fs.getAppHomeDirectory() + ".crt_offered");
	const bool offerCrt = engine.getUpscaler() != &engine.getCrt() &&
						  !fs.fileExists(crtOfferedPath);
	if(offerCrt) gui["Menu.CrtPane.Crt"]->focus();

	// When did the donation question last come up?
	const std::string lastAskedForDonationStr = fs.fileExists(fs.getAppHomeDirectory() + ".donation_asked") ? fs.readStringFromFile(fs.getAppHomeDirectory() + ".donation_asked") : "";
	if(lastAskedForDonationStr != "disable")
	{
		const uint lastAskedForDonation = static_cast<uint>(atoi(lastAskedForDonationStr.c_str()));
		const uint timePlayed = engine.getTimePlayed();

		// Unsigned: a .donation_asked larger than the time played would wrap
		// the difference and ask at every start. Never together with the CRT
		// offer; the donation question then waits for the next start.
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
	// A tick, then key and data pairs up to a ~0, then the next tick. Every
	// read is checked: isEOF() only turns true once a read has run past the
	// end, and a failed read leaves its variable as it was - a file that
	// stopped between a key and its ~0 would push that key for ever.
	File* p_file = FileSystem::inst().openFile("demo1.dat", FileSystem::FM_READ);
	if(p_file)
	{
		uint t;
		while(p_file->read(&t, 4) == 4)
		{
			uint key;
			int data;
			while(p_file->read(&key, 4) == 4 && key != ~0u && p_file->read(&data, 4) == 4)
			{
				keyData[t].push_back(translateRecordedKey(key));
				keyData[t].push_back(data);
			}
		}
		FileSystem::inst().closeFile(p_file);
	}

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

	// Give up a file dialog that is still open, or in the browser it keeps
	// the channel occupied until the page's five-minute timeout.
	Transfer::abandonImport();

	// And a question still open, which Ctrl+Shift+F2 leaves the menu under:
	// its answer could come to nothing now, and a confirmMode left standing
	// would keep pollImport() waiting on it for the rest of the run.
	confirmMode = CONFIRM_NONE;
	pendingDeleteName = "";
	pendingImportPath = "";
	pendingImportName = "";
	pendingExport = false;

	// delete the menu
	delete gui["Menu"];
	delete p_options;
	p_options = 0;
	delete p_help;
	p_help = 0;
}

void GS_Menu::onGetFocus()
{
	engine.playMusic("menu.ogg", 0.0f, true);

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
		engine.crossfade(new CF_Star, 0.85f);
	}
	else if(name == "Menu.LevelEditor")
	{
		engine.pushGameState("GS_LevelEditor");
		engine.crossfade(new CF_Star, 0.85f);
	}
	else if(name == "Menu.CampaignEditor")
	{
		engine.pushGameState("GS_CampaignEditor");
		engine.crossfade(new CF_Star, 0.85f);
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
		// Re-read on a change of kind and on request: the directory can change
		// while the pane is open.
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
		// One file, five possible kinds - Transfer::classify works out which
		// from the content. The pane stays open meanwhile.
		if(!Transfer::beginImport()) engine.showToast(Engine::TOAST_ERROR, "$TR_ERROR_CLICK_AGAIN");
	}
	else if(name == "Menu.ManagerPane.Manager.Export")
	{
		GUI_ListBox* p_list = static_cast<GUI_ListBox*>(gui["Menu.ManagerPane.Manager.Items"]);
		if(p_list->getSelection() == -1)
		{
			// Nothing selected. The button is deactivated then, and the list
			// submits only a selection; this is the guard behind both.
			engine.showToast(Engine::TOAST_ERROR, "$TR_NOTHING_TO_EXPORT");
			return;
		}

		// Only note it down; the dialog runs a round later in pollExport(), as
		// the import's does. The pane stays open: the Manager is a place you
		// keep working in.
		pendingExportKind = currentManagerKind();
		pendingExportName = p_list->getSelectedItemText();
		pendingExport = true;
	}
	else if(name == "Menu.ManagerPane.Manager.Delete")
	{
		GUI_ListBox* p_list = static_cast<GUI_ListBox*>(gui["Menu.ManagerPane.Manager.Items"]);
		if(p_list->getSelection() == -1) return;

		// The one thing in the Manager that cannot be undone, hence the
		// question; what gets deleted is settled here.
		confirmMode = CONFIRM_DELETE;
		pendingDeleteKind = currentManagerKind();
		pendingDeleteName = p_list->getSelectedItemText();

		// Deleting the progress is not deleting a file that can be imported
		// again, and the question says so.
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
		// A refused import still has to be let go of: in the browser its
		// bytes lie in a staging file that nothing else deletes once
		// pollImport() has handed it over.
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
		// _blank keeps the game running in its own tab. The click is only a
		// frame back, so the popup blocker lets the window through.
		EM_ASM({ window.open(UTF8ToString($0), "_blank"); }, "https://www.david-scherfgen.de/");
#elif defined(_WIN32)
		// As with the donate button, under Windows the address lives in a .url
		// file beside the application, which is in the Start menu too.
		ShellExecuteA(0, "open", "Scherfgen-Software Website.url", 0, 0, SW_SHOWMAXIMIZED);
#else
		openURL("https://www.david-scherfgen.de/");
#endif
	}
	else if(name == "Menu.Credits")
	{
		// The invisible button over the Credits line in the background image.
		// No ParameterBlock on purpose: GS_Credits::onEnter then asks
		// Campaign::isBuiltInCompleted(), so a player who has finished the
		// shipped campaign gets the ending. The star is the transition the
		// menu uses everywhere, and the one the credits come back through.
		engine.setGameState("GS_Credits");
		engine.crossfade(new CF_Star, 0.85f);
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

		// Asked either way. The marker records only that; its contents are
		// read nowhere.
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
			// No .url shortcut beside the application here; the address must
			// match the one in "Donate (<language>).url".
			const std::string url = engine.getLanguage() == "de"
				? "https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=UUFVK97YL6ZHY"
				: "https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=FMADXSNPDGRCW";
#ifdef __EMSCRIPTEN__
			// As for the website: _blank, and the click is only a frame back.
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
	// Not while a question is open: Transfer::pollImport() hands its answer
	// over once, and taking it now would put a second question over the
	// first. It waits in Transfer until the open one is settled.
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
										   : status == Transfer::STATUS_NO_DIALOG ? "$TR_ERROR_NO_DIALOG"
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

	// Something of the player's would be gone. Ask first, and hold the whole
	// import back until the answer - finishImport() included, since in the
	// browser it deletes the staging file the bytes are in.
	if(Transfer::wouldReplace(kind, untrustedName))
	{
		confirmMode = CONFIRM_OVERWRITE;
		pendingImportKind = kind;
		pendingImportPath = path;
		pendingImportName = untrustedName;

		// Only a progress database can be merged into the one already there.
		const bool progress = (kind == Transfer::KIND_PROGRESS);
		// Named after what they do: yes and no are no answer to a question
		// that offers replacing and merging.
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

	// A Manager still open shows the list the file has just moved into:
	// switch to its kind, re-read the list and select the new entry.
	if(gui["Menu.ManagerPane"]->isVisible())
	{
		setManagerKind(kind);
		refreshManagerList();

		GUI_ListBox* p_list = static_cast<GUI_ListBox*>(gui["Menu.ManagerPane.Manager.Items"]);
		const int where = p_list->findItem(name);
		if(where != -1) p_list->setSelection(where);
		updateManagerButtons();
	}

	// A replacement is the message: the file under this name before is gone.
	if(replaced)
	{
		engine.showToast(Engine::TOAST_OK, localizeString("$TR_REPLACED") + " \"" + name + "\"");
		return;
	}

	// A level, music or skin is reported with the name it was given, which
	// the player will enter somewhere next. It is [A-Za-z0-9_-] plus the
	// extension, so it can hold no localization marker.
	switch(kind)
	{
	case Transfer::KIND_CAMPAIGN:
		engine.showToast(Engine::TOAST_OK, "$TR_IMPORTED_CAMPAIGN");
		break;
	case Transfer::KIND_MUSIC:
		engine.showToast(Engine::TOAST_OK, localizeString("$TR_IMPORTED_MUSIC") + " \"" + name + "\"");
		break;
	case Transfer::KIND_SKIN:
		// Without ".zip" and the dot, as the name goes into a skin slot;
		// setFilenameExtension would leave the dot in place.
		engine.showToast(Engine::TOAST_OK, localizeString("$TR_IMPORTED_SKIN") + " \"" +
										   name.substr(0, name.find_last_of('.')) + "\"");
		break;
	case Transfer::KIND_PROGRESS:
		// No name: there is only the one file, and "progress.zip" would tell
		// the player nothing they can use.
		engine.showToast(Engine::TOAST_OK, "$TR_IMPORTED_PROGRESS");
		break;
	default:
		engine.showToast(Engine::TOAST_OK, localizeString("$TR_IMPORTED_LEVEL") + " \"" + name + "\"");
		break;
	}
}

void GS_Menu::mergeImport(const std::string& path)
{
	// classify() only asked the table of contents for a progress.xml. Reading
	// it can still fail, and an empty union would then pass for a merge that
	// worked.
	if(!ProgressDB::canRead(path))
	{
		Transfer::finishImport();
		engine.showToast(Engine::TOAST_ERROR, "$TR_ERROR_BROKEN");
		return;
	}

	// The union needs no code of its own: mark everything in the imported
	// database solved, and markSolved() adds it to the player's own.
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
	// just read from.
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

	// Back to the Manager only if it is still open: focus() shows every
	// parent of what it focuses, and would reopen a pane the player closed
	// while an asynchronous file dialog was up.
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
	// check() and not setChecked(): as if clicked. It fires only on a change,
	// so the caller re-reads the list anyway.
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

	// By hand: setSelection() reports only a change of index, and the
	// content can differ under the same index.
	updateManagerButtons();
}

void GS_Menu::updateManagerButtons()
{
	GUI_ListBox* p_list = static_cast<GUI_ListBox*>(gui["Menu.ManagerPane.Manager.Items"]);
	const bool haveSelection = p_list->getSelection() != -1;

	GUI_Button* p_export = static_cast<GUI_Button*>(gui["Menu.ManagerPane.Manager.Export"]);
	if(haveSelection) p_export->activate();
	else              p_export->deactivate();

	// What the game ships is listed and can be exported, but not deleted: the
	// skin a level names would be gone, with nothing to replace it. An
	// example level has nothing to delete until the player has saved one.
	const bool canDelete = haveSelection &&
						   Transfer::isRemovable(static_cast<Transfer::Kind>(currentManagerKind()),
												 p_list->getSelectedItemText());

	GUI_Button* p_delete = static_cast<GUI_Button*>(gui["Menu.ManagerPane.Manager.Delete"]);
	if(canDelete) p_delete->activate();
	else          p_delete->deactivate();
}
