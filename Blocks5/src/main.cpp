#include "pch.h"
#include "engine.h"
#include "filesystem.h"
#include "gs_menu.h"
#include "gs_selectlevel.h"
#include "gs_game.h"
#include "gs_leveleditor.h"
#include "gs_campaigneditor.h"
#include "gs_credits.h"
#include "gs_loading.h"
#include "gui.h"
#include "cf_all.h"
#include "updatecheck.h"
#ifdef __EMSCRIPTEN__
#include "web_transfer.h"
#endif
#ifdef _WIN32
#include "stackwalker.h"
#endif

const char* p_localVersion = "1.2.0";

#ifdef _WIN32
class MyStackWalker : public StackWalker
{
public:
	MyStackWalker() : StackWalker() {}
	MyStackWalker(DWORD processID, HANDLE process) : StackWalker(processID, process) {}
	virtual void OnOutput(LPCSTR p_text) { printfLog("%s\n", p_text); StackWalker::OnOutput(p_text); }
};

LONG WINAPI expFilter(EXCEPTION_POINTERS* p_exception,
					  DWORD exceptionCode)
{
	printfLog("\n");
	writingCrashLog = true;
	printfLog("**************************************************\n");
	printfLog("An exception was thrown!\n");
	printfLog("Exception code: %x\n", exceptionCode);
	printfLog("Printing stack trace ...\n");
	printfLog("**************************************************\n\n");

	MyStackWalker sw;
	sw.ShowCallstack(GetCurrentThread(), p_exception->ContextRecord);

	return EXCEPTION_EXECUTE_HANDLER;
}
#endif // _WIN32

namespace
{
#ifndef __EMSCRIPTEN__
	// The update check's switch is <CheckForUpdates> in config.xml, and where
	// that does not say yet, the installation's default stands in for it
	// (Engine::loadConfig). A .update_checker in the user directory - '1' for
	// on, anything else for off - is the switch a version before 1.2.0 kept
	// there: taken into config.xml and deleted, but deleted only once
	// config.xml is seen written, or a full disk would lose the choice.
	void adoptUpdateCheckChoice(Engine& engine, FileSystem& fs)
	{
		const std::string path(fs.getAppHomeDirectory() + ".update_checker");
		if(!fs.fileExists(path)) return;

		const std::string choice(fs.readStringFromFile(path));
		engine.setCheckForUpdates(!choice.empty() && choice[0] == '1');
		printfLog("Update check %s, as \"%s\" says.\n",
				  engine.getCheckForUpdates() ? "on" : "off", path.c_str());

		if(!engine.saveConfig() || !fs.fileExists(fs.getAppHomeDirectory() + "config.xml"))
		{
			printfLog("+ WARNING: config.xml could not be written; \"%s\" stays.\n", path.c_str());
		}
		else if(!fs.deleteFile(path))
		{
			printfLog("+ WARNING: Could not delete \"%s\".\n", path.c_str());
		}
	}
#endif
}

// Set aside copies of shipped files in the user directory, where earlier
// versions copied the shipped content on the first start. The game folder
// comes first, so such a copy could never be loaded, and the Manager, which
// lists the name once as the shipped file's, would not delete it. The player
// files (FileSystem::getPlayerFiles) are not shipped content and stay.
//
// Renamed, not deleted: nobody can tell from outside whether one was edited.
// .bak takes them out of every list, since every lister filters on the exact
// extension, and out of the archive path, since FileSystem::convertPath()
// recognises an archive by ".zip/", not ".zip".
uint retireShadowingCopies(FileSystem& fs, const std::string& homeDirectory)
{
	static const char* p_subdirs[] = { "levels/", "levels/campaigns/", "levels/skins/" };
	uint retired = 0;

	for(uint s = 0; s < sizeof(p_subdirs) / sizeof(p_subdirs[0]); s++)
	{
		const std::string sub(p_subdirs[s]);
		const std::list<std::string> files(fs.listDirectory(homeDirectory + sub));

		for(std::list<std::string>::const_iterator i = files.begin(); i != files.end(); ++i)
		{
			if(!fs.isShippedContent(sub + *i)) continue;

			// An existing .bak gives way: it comes from an earlier run and
			// means the same file.
			const std::string from(homeDirectory + sub + *i);
			const std::string to(from + ".bak");
			if(fs.renameFile(from, to))
			{
				printfLog("* Retired the old copy of \"%s%s\" as \"%s.bak\".\n",
						  sub.c_str(), i->c_str(), i->c_str());
				retired++;
			}
			else
			{
				printfLog("+ WARNING: Could not retire \"%s\".\n", from.c_str());
			}
		}
	}

#ifdef __EMSCRIPTEN__
	// Write through to IndexedDB at once, as after an import, a delete or an
	// editor's save, rather than at the next five-second sync: otherwise the
	// old copies would stand there again after a reload, and the run would
	// have the same work to do at every start.
	if(retired) WebTransfer::syncHome();
#endif

	return retired;
}

const std::string detectInitializedVersion()
{
	// not_played:	no "Blocks 5" folder in the user directory and no "progress.zip" file in the working directory
	// <= 1.0.7:	no "Blocks 5" folder in the user directory
	//    1.0.71:	"Blocks 5" folder exists in the user directory
	//    1.0.72:	".initialized" file exists
	// >= 1.0.73:	".initialized" file holds the version number
	// A "Blocks 5" folder with no file directly in it counts as none.

	FileSystem& fs = FileSystem::inst();
	const std::string homeDirectory(fs.getAppHomeDirectory());

	if(fs.listDirectory(homeDirectory).empty())
	{
		// TODO: a progress.zip that UAC redirected into the VirtualStore is
		// not found here.
		if(!fs.fileExists("progress.zip")) return "not_played";
		else return "<= 1.0.7";
	}
	else if(!fs.fileExists(homeDirectory + ".initialized")) return "1.0.71";
	else
	{
		const std::string content(fs.readStringFromFile(homeDirectory + ".initialized"));
		if(content.empty()) return "1.0.72";
		else return content;
	}
}

int runTheGame(int argc,
			   char** pp_argv)
{
	FileSystem& fs = FileSystem::inst();
	const std::string homeDirectory(fs.getAppHomeDirectory());
	const std::string versionInitialized(detectInitializedVersion());
	fs.createDirectory(homeDirectory);

	clearLog();
	printfLog("Blocks 5\n");
	printfLog("========\n");
	printfLog("Installed game version: %s\n", p_localVersion);
	printfLog("Last played:            %s\n", versionInitialized.c_str());

	if(versionInitialized != p_localVersion)
	{
		printfLog("Initializing/Updating ...\n");

		bool success = true;
		std::string errorMsg;
		bool severeError = false;

		if(versionInitialized == "not_played" ||
		   versionInitialized == "<= 1.0.7")
		{
			// initialize the directory
			success &= fs.createDirectory(homeDirectory + "levels");
			success &= fs.createDirectory(homeDirectory + "levels/campaigns");
			success &= fs.createDirectory(homeDirectory + "levels/skins");
			success &= fs.createDirectory(homeDirectory + "screenshots");
			success &= fs.createDirectory(homeDirectory + "videos");
			if(versionInitialized == "<= 1.0.7") success &= fs.copyFile("progress.zip", homeDirectory + "progress.zip");

			// No config.xml: the game writes its own, and a template's
			// <Language> would overrule the one
			// Engine::detectSystemLanguage() finds, for every player alike.

			// The campaign and the skins stay in the game folder, always as new
			// as the program; the folders above are where the editors and the
			// import write. Of the player files only the five readme.txt are
			// copied: nothing reads them, so without the copy they would be in
			// no folder at all. The example levels load from the game folder
			// until the player saves their own.
			for(const FileSystem::PlayerFile* p_file = FileSystem::getPlayerFiles();
				p_file->p_path; p_file++)
			{
				if(!p_file->copyOnFirstStart) continue;
				success &= fs.copyFile(fs.getGameDirectory() + p_file->p_path,
									   homeDirectory + p_file->p_path);
			}

			if(success)
			{
				if(versionInitialized != "not_played")
				{
#ifdef _WIN32
					int answer = MessageBoxA(0,
											 "In the new version, Blocks 5 stores the levels, campaigns and other data in a different folder. "
											 "These files are now in a folder called \"Blocks 5\" within your \"My Documents\" folder. "
											 "Please keep this in mind when installing new levels, campaigns or skins!\r\n\r\n"
											 "Do you want to open this folder now in Windows Explorer?",
											 "Important update information",
											 MB_YESNO | MB_ICONINFORMATION);
					if(answer == IDYES)
					{
						std::string temp(homeDirectory);
						for(std::string::iterator it = temp.begin(); it != temp.end(); ++it) if(*it == '/') *it = '\\';
						const std::string cmdLine = std::string("EXPLORER.EXE \"") + temp + "\"";
						WinExec(cmdLine.c_str(), SW_SHOWMAXIMIZED);
						Sleep(5000);
						MessageBoxA(0,
									"Windows Explorer has been started, you should now see the new folder. "
									"Click OK to continue.",
									"Continue",
									MB_OK | MB_ICONINFORMATION);
					}
#endif
				}
			}
			else
			{
				severeError = true;
				errorMsg = std::string("The program could not create and initialize the folder: \"") + homeDirectory + "\"";
			}
		}
		else if(versionInitialized == "1.0.71" ||
				versionInitialized == "1.0.72")
		{
			fs.deleteFile(homeDirectory + "updates.no");

			success &= fs.createDirectory(homeDirectory + "videos");
			success &= fs.copyFile("videos/readme.txt", homeDirectory + "videos/readme.txt");

			if(!success) errorMsg = "Could not migrate all settings!";
		}
		else if(versionInitialized == "1.0.73")
		{
			fs.deleteFile(homeDirectory + "updates.no");
		}

		// On every version change, not only on the jump to 1.2.0: the run
		// costs nothing when there is nothing to do, and a player going from
		// 1.2.0 to 1.2.1 can bring along a copy that 1.2.0 could not clear
		// away. On a fresh installation there is nothing to find.
		if(versionInitialized != "not_played") retireShadowingCopies(fs, homeDirectory);

#ifdef _WIN32
		// The two .bat files that switched the update check before 1.2.0, and
		// which a first start copied into the user directory. The switch is
		// the options dialog's now, and one of them run would only leave a
		// .update_checker for the next start to take in.
		fs.deleteFile(homeDirectory + "update_checker_disable.bat");
		fs.deleteFile(homeDirectory + "update_checker_enable.bat");
#endif

		if(success)
		{
			fs.writeStringToFile(p_localVersion, homeDirectory + ".initialized");
			printfLog("Succeeded initializing/updating user directory!\n");

#ifdef _WIN32
			if(versionInitialized != "not_played")
			{
				int answer = MessageBoxA(0,
										 "Do you want to read the changelog (what's new in this version)? "
										 "If you click \"Yes\", the changelog will be opened in Notepad. "
										 "Once you close the window, the game will start.",
										 "Read changelog?",
										 MB_YESNO | MB_ICONQUESTION);
				if(answer == IDYES)
				{
					system("NOTEPAD.EXE readme.txt");
					Sleep(1000);
				}
			}
#endif
		}
		else
		{
#ifdef _WIN32
			MessageBoxA(0, errorMsg.c_str(), "Error!", MB_OK | MB_ICONERROR);
#else
			std::cerr << errorMsg.c_str() << std::endl;
#endif
			printfLog("%s\n", errorMsg.c_str());
			if(severeError) return 1;
		}
	}

	// read the data out of the encrypted archive
	fs.pushCurrentDir("data.zip[3Cs18Ab0bV0Aat3Wf27le1ZM12kt0Xs05Aa4PX1EyI2V112Jr26v2GZO3dN0Ec91hk024P3cA32bc3GZ07Em4bf34st4320F7d13S00wd4Mg1ANn4SF2EO94Hz13Qq0LO18iY4Qy2C8r2XF28Bh]");

	// Alternatively: read the data from the local directory
	// fs.pushCurrentDir("data");

	bool fullScreen;

#ifdef _DEBUG
	fullScreen = false;
#else
	fullScreen = true;
#endif

	// Parse the arguments. The upscaling filter is deliberately not among
	// them: it is a setting like the language and is chosen in the options
	// dialog.
	Engine& engine = Engine::inst();

	for(int i = 0; i < argc; i++)
	{
		char* p_arg = pp_argv[i];
		if(equalsNoCase(p_arg, "-windowed")) engine.overrideFullScreen(false);
		else if(equalsNoCase(p_arg, "-fullScreen")) engine.overrideFullScreen(true);
		else if(equalsNoCase(p_arg, "-noSplash")) engine.skipSplash();
		else if(equalsNoCase(p_arg, "-perf")) engine.showPerformance();
		else if(equalsNoCase(p_arg, "-flushAll")) engine.enableFlushAll();
#ifndef __EMSCRIPTEN__
		// In no readme: the author's way to see an update offered without
		// publishing one. The update check alone takes the version that
		// follows for the one running; .initialized was written above with
		// p_localVersion, and the migration went by that too.
		else if(equalsNoCase(p_arg, "-updateCheckVersion") && i + 1 < argc) UpdateCheck::setVersion(pp_argv[++i]);
#endif
	}

	printfLog("Initializing engine ...\n");

	// define the game actions
	//
	// The six the mouse drag feeds take it as a third source: both real slots
	// are taken and worth keeping, and a gesture is its own binding, never
	// offered in the options dialog or written to config.xml. Player only
	// asks for the action and never learns that a mouse can steer it.
	Action* p_action = engine.registerAction("$A_LEFT", engine.getKeyboardVK(SDLK_LEFT), engine.getKeyboardVK(SDLK_KP4));
	p_action->resetsActions.push_back("$A_UP");
	p_action->resetsActions.push_back("$A_DOWN");
	p_action->tertiary = engine.getMouseDragVK(Engine::MOUSE_DRAG_LEFT);
	p_action = engine.registerAction("$A_RIGHT", engine.getKeyboardVK(SDLK_RIGHT), engine.getKeyboardVK(SDLK_KP6));
	p_action->resetsActions.push_back("$A_UP");
	p_action->resetsActions.push_back("$A_DOWN");
	p_action->tertiary = engine.getMouseDragVK(Engine::MOUSE_DRAG_RIGHT);
	p_action = engine.registerAction("$A_UP", engine.getKeyboardVK(SDLK_UP), engine.getKeyboardVK(SDLK_KP8));
	p_action->resetsActions.push_back("$A_LEFT");
	p_action->resetsActions.push_back("$A_RIGHT");
	p_action->tertiary = engine.getMouseDragVK(Engine::MOUSE_DRAG_UP);
	p_action = engine.registerAction("$A_DOWN", engine.getKeyboardVK(SDLK_DOWN), engine.getKeyboardVK(SDLK_KP2));
	p_action->resetsActions.push_back("$A_LEFT");
	p_action->resetsActions.push_back("$A_RIGHT");
	p_action->tertiary = engine.getMouseDragVK(Engine::MOUSE_DRAG_DOWN);
	p_action = engine.registerAction("$A_PLANT_BOMB", engine.getKeyboardVK(SDLK_LSHIFT), engine.getKeyboardVK(SDLK_RSHIFT));
	p_action->tertiary = engine.getMouseDragVK(Engine::MOUSE_DRAG_PLANT);
	p_action = engine.registerAction("$A_PUT_DOWN_BOMB", engine.getKeyboardVK(SDLK_LCTRL), engine.getKeyboardVK(SDLK_RCTRL));
	p_action->tertiary = engine.getMouseDragVK(Engine::MOUSE_DRAG_PUT_DOWN);
	// Once per press: on the default repeat (240 ms, then every 80) a held
	// Tab would cycle the active character for as long as it was held.
	p_action = engine.registerAction("$A_SWITCH_CHARACTER", engine.getKeyboardVK(SDLK_TAB));
	p_action->repeats = false;
	engine.registerAction("$A_SAVE_IN_HOTEL", engine.getKeyboardVK(SDLK_RETURN), engine.getKeyboardVK(SDLK_KP_ENTER));
	// The two restarts and the pause fire once per press, like the toggles
	// below. A repeating action buffers presses made during its delay, five
	// deep, and plays them out later: mashing F5 would go on restarting after
	// the last press, each rewind beginning again over the one running, and a
	// held pause key would toggle on and off.
	p_action = engine.registerAction("$A_RESTART_LEVEL", engine.getKeyboardVK(SDLK_F5));
	p_action->repeats = false;
	p_action = engine.registerAction("$A_RESTART_FROM_HOTEL", engine.getKeyboardVK(SDLK_F10));
	p_action->repeats = false;
	p_action = engine.registerAction("$A_PAUSE", engine.getKeyboardVK(SDLK_PAUSE));
	p_action->repeats = false;

	// define the engine actions
	// Toggles, not auto-fire actions: once per press.
	p_action = engine.registerAction("$A_TOGGLE_MUTE", engine.getKeyboardVK(SDLK_F1));
	p_action->repeats = false;
	p_action = engine.registerAction("$A_CAPTURE_SCREENSHOT", engine.getKeyboardVK(SDLK_F11));
	p_action->repeats = false;
#ifndef __EMSCRIPTEN__
	// No video recording in the web build, so the action is not registered
	// there and does not stand uselessly in the binding list; Engine::update
	// may still ask, since an unknown action is never pressed. Screenshots
	// exist there too, downloaded instead of stored.
	p_action = engine.registerAction("$A_TOGGLE_CAPTURE_VIDEO", engine.getKeyboardVK(SDLK_F12));
	p_action->repeats = false;
#endif

	if(!engine.init("Blocks 5", "window.png", 640, 480, fullScreen))
	{
		printfLog("Error while initializing the engine.\n");
		return 1;
	}

#ifndef __EMSCRIPTEN__
	// After init(), which has read config.xml, and as early as that: the menu
	// shows the answer, and it asks for it once a tick.
	adoptUpdateCheckChoice(engine, fs);
	if(engine.getCheckForUpdates()) UpdateCheck::start();
	else printfLog("Not checking for updates.\n");
#endif

	// load the localization
	engine.loadStringDB("languages.txt");

	// Before the sounds are loaded: Sound looks its factor up at construction.
	engine.loadSoundVolumes("sounds.xml");

	// create instances of the game state classes
	GS_Menu menu;
	GS_SelectLevel selectLevel;
	GS_Game game;
	GS_LevelEditor levelEditor;
	GS_CampaignEditor campaignEditor;
	GS_Credits credits;
	GS_Loading loading;

	printfLog("Starting game ...\n");
	engine.setGameState("GS_Loading");

	printfLog("Entering main loop ...\n");
	engine.mainLoop();

#ifndef __EMSCRIPTEN__
	// A check still running ends with the game, not after it.
	UpdateCheck::abort();
#endif

	printfLog("Shutting down the engine ...\n");
	engine.exit();
	printfLog("Engine has been shut down.\n");

	return 0;
}

int main(int argc,
		 char** pp_argv)
{
	// TODO: take http://blog.kalmbachnet.de/?postid=75 into account (StackWalker homepage: http://stackwalker.codeplex.com/releases/view/35258)

	// The crash handler is SEH and therefore Windows-only. In a Debug build
	// it is not put in front even there, because it would then catch the error
	// before the debugger does.
#if defined(_WIN32) && !defined(_DEBUG)
	__try
	{
		return runTheGame(argc, pp_argv);
	}
	__except(expFilter(GetExceptionInformation(), GetExceptionCode()))
	{
		// The trace is in the log, which printfLog() closes after every line.
		// Nothing after a crash is to be trusted, so the process ends here:
		// returning would run the static destructors, the engine's whole
		// shutdown among them, over whatever the crash damaged. What that
		// gives up is the window placement and a video being recorded, whose
		// index is written only when it closes.
		TerminateProcess(GetCurrentProcess(), 1);
		return 1;
	}
#else
	return runTheGame(argc, pp_argv);
#endif
}