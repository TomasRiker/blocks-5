#include "pch.h"
#include "engine.h"
#include "filesystem.h"
#include "transfer.h"
#include "gs_menu.h"
#include "gs_selectlevel.h"
#include "gs_game.h"
#include "gs_leveleditor.h"
#include "gs_campaigneditor.h"
#include "gs_credits.h"
#include "gs_loading.h"
#include "gui.h"
#include "cf_all.h"
#include "progressdb.h"
#ifdef __EMSCRIPTEN__
#include "web_transfer.h"
#endif
#ifdef _WIN32
#include "stackwalker.h"
#endif

#ifdef _WIN32
#include <shellapi.h>
#include <wininet.h>
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

std::string getCurrentVersion()
{
#ifdef _WIN32
	struct Task
	{
		Task()
		{
			currentVersion = "";
			finished = false;
		}

		static DWORD WINAPI threadProc(void* p_param)
		{
			// The version number belongs in the agent string: the server log
			// then says which version is asking. Older installations still send
			// the bare name without the bracket.
			const std::string agent = std::string("Scherfgen-Software Blocks 5 (") + p_localVersion + ")";
			HINTERNET inet = InternetOpenA(agent.c_str(), INTERNET_OPEN_TYPE_PRECONFIG, 0, 0, 0);
			if(!inet) return 1;

			// InternetOpenUrl does not need to be told INTERNET_FLAG_SECURE -
			// it reads the scheme from the address - but written out it shows
			// that https is deliberate here.
			HINTERNET url = InternetOpenUrlA(inet, "https://www.david-scherfgen.de/stuff/blocks-5/version.txt",
											 0, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE, 0);
			if(!url)
			{
				InternetCloseHandle(inet);
				return 1;
			}

			char buffer[17] = {0};
			DWORD numBytesRead = 0;
			if(!InternetReadFile(url,
				buffer,
				16,
				&numBytesRead))
			{
				InternetCloseHandle(url);
				InternetCloseHandle(inet);
				return 1;
			}

			buffer[numBytesRead] = 0;
			if(numBytesRead == 16) buffer[0] = 0;

			InternetCloseHandle(url);
			InternetCloseHandle(inet);

			Task& task = *reinterpret_cast<Task*>(p_param);
			task.currentVersion = buffer;
			task.finished = true;

			return 0;
		}

		std::string currentVersion;
		bool finished;
	};

	// Run the query in a thread and allow it two seconds at most
	Task task;
	DWORD threadID;
	HANDLE thread = CreateThread(0, 0, Task::threadProc, &task, 0, &threadID);
	WaitForSingleObject(thread, 2000);
	return task.finished ? task.currentVersion : "";
#elif defined(__EMSCRIPTEN__)
	return "";  // no update check in the browser build
#else
	// No HTTP client of our own: that would be TLS, and linking one more
	// library for sixteen bytes is not worth it. curl and wget are on just
	// about every Linux, and where neither is, the query simply does not
	// happen - it can be switched off anyway and is off as shipped.
	const std::string agent = std::string("Scherfgen-Software Blocks 5 (") + p_localVersion + ")";
	const char* const p_url = "https://www.david-scherfgen.de/stuff/blocks-5/version.txt";

	// The two-second limit is the same as under Windows, but here it is the
	// job of the program that makes the query. Both write only to stdout and
	// are otherwise silent.
	std::string command = "curl -fsS --max-time 2 -A '" + agent + "' '" + p_url + "' 2>/dev/null";
	if(::system("command -v curl >/dev/null 2>&1") != 0)
	{
		command = "wget -q -T 2 -t 1 -U '" + agent + "' -O - '" + p_url + "' 2>/dev/null";
		if(::system("command -v wget >/dev/null 2>&1") != 0) return "";
	}

	FILE* p_pipe = ::popen(command.c_str(), "r");
	if(!p_pipe) return "";

	char buffer[17] = {0};
	const size_t numBytesRead = ::fread(buffer, 1, 16, p_pipe);
	::pclose(p_pipe);

	// As under Windows: sixteen bytes mean there is more there than a
	// version number - then the answer is not the one expected.
	if(numBytesRead == 16) return "";
	buffer[numBytesRead] = 0;
	return buffer;
#endif
}

namespace
{
	// The two switches for the update checker, needed three times - at the
	// first installation and on two update paths. The .bat files ship only
	// under Windows: a batch file cannot be run anywhere else, and
	// .update_checker is a text file with one character in it that any
	// editor can change.
	bool copyUpdateCheckerFiles(FileSystem& fs, const std::string& homeDirectory)
	{
		bool success = true;
#ifdef _WIN32
		success &= fs.copyFile("update_checker_disable.bat", homeDirectory + "update_checker_disable.bat");
		success &= fs.copyFile("update_checker_enable.bat", homeDirectory + "update_checker_enable.bat");
#endif
		if(fs.fileExists(".update_checker")) success &= fs.copyFile(".update_checker", homeDirectory + ".update_checker");
		return success;
	}

	// "1.2.0" to 1002000, and anything that is not a version number to -1.
	// Up to three groups, a missing one counts as 0, whitespace before and
	// after is allowed.
	long parseVersion(const std::string& text)
	{
		size_t i = 0;
		while(i < text.length() && isspace(static_cast<unsigned char>(text[i]))) ++i;

		long part[3] = { 0, 0, 0 };
		int n = 0;
		bool anyDigit = false;
		while(n < 3)
		{
			if(i >= text.length() || !isdigit(static_cast<unsigned char>(text[i]))) break;
			long value = 0;
			while(i < text.length() && isdigit(static_cast<unsigned char>(text[i])))
			{
				value = value * 10 + (text[i++] - '0');
				if(value > 999) return -1;
			}
			part[n++] = value;
			anyDigit = true;
			if(i < text.length() && text[i] == '.') ++i;
			else break;
		}

		while(i < text.length() && isspace(static_cast<unsigned char>(text[i]))) ++i;
		if(!anyDigit || i != text.length()) return -1;

		return part[0] * 1000000 + part[1] * 1000 + part[2];
	}
}

bool isNewer(const std::string& version1,
			 const std::string& version2)
{
	// Compare as numbers and not as strings: a trailing newline, an error
	// page from the server or simply "1.10.0" against "1.9.0" would
	// otherwise produce an "update available" for somebody who has the
	// newest version. Anything that cannot be read as a version number is
	// never newer - a suffix like "1.3.0-beta" therefore falls through too.
	const long v1 = parseVersion(version1);
	const long v2 = parseVersion(version2);
	if(v1 < 0 || v2 < 0) return false;
	return v1 > v2;
}

// Set aside the copies of the shipped files in the user directory.
//
// Older installations copied everything the game ships into the user
// directory once, on the first start. It lives in the game folder now and is
// read from there, and the game folder comes first: those old copies would
// be unreachable and would only appear twice in the Manager's list.
//
// Renamed and not deleted: from outside there is no telling whether somebody
// has changed one of them. A player who called their own level
// "example01.xml" must be able to find it again. The extension .bak takes them
// out of every list, because every lister filters on the exact extension, and
// out of the archive path as well: FileSystem::convertPath() recognises an
// archive by ".zip/", not by ".zip".
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

			// The game's filesystem has no rename: copy, then delete the
			// original. An existing .bak gives way: it comes from an earlier
			// run and means the same file.
			const std::string from(homeDirectory + sub + *i);
			const std::string to(from + ".bak");
			fs.deleteFile(to);
			if(fs.copyFile(from, to) && fs.deleteFile(from))
			{
				printfLog("* Retired the old copy of \"%s%s\" as \"%s.bak\".\n",
						  sub.c_str(), i->c_str(), i->c_str());
				retired++;
			}
			else
			{
				printfLog("+ WARNING: Could not retire \"%s\".\n", from.c_str());
				fs.deleteFile(to);
			}
		}
	}

#ifdef __EMSCRIPTEN__
	// Write through to IndexedDB at once, as after every other write in the
	// browser: otherwise the old copies would stand there again after a
	// reload, and the run would have the same work to do at every start.
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

	FileSystem& fs = FileSystem::inst();
	const std::string homeDirectory(fs.getAppHomeDirectory());

	if(fs.listDirectory(homeDirectory).empty())
	{
		// TODO: The "progress.zip" from the VirtualStore is not found! Why not? It used to work!
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
		bool quit = false;

		if(versionInitialized == "not_played" ||
		   versionInitialized == "<= 1.0.7")
		{
			// initialize the directory
			success &= fs.createDirectory(homeDirectory + "levels");
			success &= fs.createDirectory(homeDirectory + "levels/campaigns");
			success &= fs.createDirectory(homeDirectory + "levels/skins");
			success &= fs.createDirectory(homeDirectory + "screenshots");
			success &= fs.createDirectory(homeDirectory + "videos");
			// The game writes config.xml itself on exit. Nothing ships a
			// template: it would hold nothing but the installer's language and
			// would never let Engine::detectSystemLanguage() run.
			if(versionInitialized == "<= 1.0.7") success &= fs.copyFile("progress.zip", homeDirectory + "progress.zip");
			success &= copyUpdateCheckerFiles(fs, homeDirectory);

			// The campaign and the skins are not copied over: they stay in the
			// game folder and are read from there, which keeps them always
			// exactly as new as the program beside them. The folders are
			// created all the same, because that is where the editors and the
			// import write.
			//
			// The five readme.txt are copied: they explain the player's own
			// folders to them, and because the game never reads them, without
			// this copy they would sit in no folder at all. The two example
			// levels stand beside them in the list and are deliberately not
			// copied - they are visible and loadable straight out of the game
			// folder, and a player who changes one and saves it gets their own
			// version by itself. Only on the very first start.
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
			success &= copyUpdateCheckerFiles(fs, homeDirectory);

			success &= fs.createDirectory(homeDirectory + "videos");
			success &= fs.copyFile("videos/readme.txt", homeDirectory + "videos/readme.txt");

			if(!success) errorMsg = "Could not migrate all settings!";
		}
		else if(versionInitialized == "1.0.73")
		{
			fs.deleteFile(homeDirectory + "updates.no");
			success &= copyUpdateCheckerFiles(fs, homeDirectory);

			if(!success) errorMsg = "Could not migrate all settings!";
		}

		// On every version change, not only on the jump to 1.2.0: the run
		// costs nothing when there is nothing to do, and a player going from
		// 1.2.0 to 1.2.1 can bring along a copy that 1.2.0 could not clear
		// away. On a fresh installation there is nothing to find.
		if(versionInitialized != "not_played") retireShadowingCopies(fs, homeDirectory);

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

		if(quit) return 0;
	}

	if(!fs.fileExists(homeDirectory + ".update_checker")) fs.writeStringToFile("0", homeDirectory + ".update_checker");
	const std::string updateCheckerStatus(fs.readStringFromFile(homeDirectory + ".update_checker"));
	if(!updateCheckerStatus.empty() && updateCheckerStatus[0] == '1')
	{
		printfLog("Checking for update ...\n");

		// is there a new version?
		std::string currentVersion = getCurrentVersion();
		if(currentVersion.empty()) printfLog("Could not detect current version!\n");
		else printfLog("Current game version:   %s\n", currentVersion.c_str());
		if(!currentVersion.empty() &&
		   isNewer(currentVersion, p_localVersion))
		{
			std::ostringstream str;
			str << "A new version of Blocks 5 is available.\r\n";
			str << "Installed version: " << p_localVersion << "\r\n";
			str << "New version: " << currentVersion << "\r\n\r\n";
			str << "Do you want to visit the Blocks 5 website now?" << "\r\n\r\n";

#ifdef _WIN32
			int answer = MessageBoxA(0, str.str().c_str(), "Update available!", MB_YESNO | MB_ICONINFORMATION);
			if(answer == IDYES)
			{
				// open the page
				ShellExecuteA(0, "open", "Blocks 5 Website.url", NULL, NULL, SW_SHOWNORMAL);
				return 0;
			}
#elif !defined(__EMSCRIPTEN__)
			// No window: the engine is not running yet, and there is therefore
			// neither a toast bar nor a dialog, and a browser nobody asked for
			// would simply spring open at startup. The query can be switched
			// off anyway and is off as shipped - anyone who switched it on did
			// that in a text file and sees these lines as well.
			printfLog("%s", str.str().c_str());
			printfLog("https://www.david-scherfgen.de/meine-spiele/blocks-5/\n\n");
#endif
		}
	}
	else
	{
		// No automatic updates!
		printfLog("Not checking for update!\n");
	}

	// read the data out of the encrypted archive
	fs.pushCurrentDir("data.zip[3Cs18Ab0bV0Aat3Wf27le1ZM12kt0Xs05Aa4PX1EyI2V112Jr26v2GZO3dN0Ec91hk024P3cA32bc3GZ07Em4bf34st4320F7d13S00wd4Mg1ANn4SF2EO94Hz13Qq0LO18iY4Qy2C8r2XF28Bh]");

	// Alternatively: read the data from the local directory
	// fs.pushCurrentDir("data");

	// load the progress
	ProgressDB::inst().load();

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
		else if(equalsNoCase(p_arg, "-noFBO")) engine.disableFrameBuffer();
		else if(equalsNoCase(p_arg, "-noShader")) engine.disableShaders();
	}

	printfLog("Initializing engine ...\n");

	// define the game actions
	Action* p_action = engine.registerAction("$A_LEFT", engine.getKeyboardVK(SDLK_LEFT), engine.getKeyboardVK(SDLK_KP4));
	p_action->resetsActions.push_back("$A_UP");
	p_action->resetsActions.push_back("$A_DOWN");
	p_action = engine.registerAction("$A_RIGHT", engine.getKeyboardVK(SDLK_RIGHT), engine.getKeyboardVK(SDLK_KP6));
	p_action->resetsActions.push_back("$A_UP");
	p_action->resetsActions.push_back("$A_DOWN");
	p_action = engine.registerAction("$A_UP", engine.getKeyboardVK(SDLK_UP), engine.getKeyboardVK(SDLK_KP8));
	p_action->resetsActions.push_back("$A_LEFT");
	p_action->resetsActions.push_back("$A_RIGHT");
	p_action = engine.registerAction("$A_DOWN", engine.getKeyboardVK(SDLK_DOWN), engine.getKeyboardVK(SDLK_KP2));
	p_action->resetsActions.push_back("$A_LEFT");
	p_action->resetsActions.push_back("$A_RIGHT");
	engine.registerAction("$A_PLANT_BOMB", engine.getKeyboardVK(SDLK_LSHIFT), engine.getKeyboardVK(SDLK_RSHIFT));
	engine.registerAction("$A_PUT_DOWN_BOMB", engine.getKeyboardVK(SDLK_LCTRL), engine.getKeyboardVK(SDLK_RCTRL));
	engine.registerAction("$A_SWITCH_CHARACTER", engine.getKeyboardVK(SDLK_TAB));
	engine.registerAction("$A_SAVE_IN_HOTEL", engine.getKeyboardVK(SDLK_RETURN), engine.getKeyboardVK(SDLK_KP_ENTER));
	p_action = engine.registerAction("$A_RESTART_LEVEL", engine.getKeyboardVK(SDLK_F5));
	p_action->delay = 1000;
	p_action->interval = 1000;
	p_action = engine.registerAction("$A_RESTART_FROM_HOTEL", engine.getKeyboardVK(SDLK_F10));
	p_action->delay = 1000;
	p_action->interval = 1000;
	p_action = engine.registerAction("$A_PAUSE", engine.getKeyboardVK(SDLK_PAUSE));
	p_action->delay = 200;
	p_action->interval = 500;

	// define the engine actions
	// Toggles, not auto-fire actions: once per press.
	p_action = engine.registerAction("$A_TOGGLE_MUTE", engine.getKeyboardVK(SDLK_F1));
	p_action->repeats = false;
	p_action = engine.registerAction("$A_CAPTURE_SCREENSHOT", engine.getKeyboardVK(SDLK_F11));
	p_action->repeats = false;
#ifndef __EMSCRIPTEN__
	// There is no video recording in the web build, and the action is
	// therefore not registered there at all - it would otherwise stand
	// uselessly in the key binding list. The query in Engine::update stays
	// unchanged: getAction() returns 0 for an unknown name. Screenshots do
	// exist there; they are only downloaded instead of stored.
	p_action = engine.registerAction("$A_TOGGLE_CAPTURE_VIDEO", engine.getKeyboardVK(SDLK_F12));
	p_action->repeats = false;
#endif

	if(!engine.init("Blocks 5", "window.png", 640, 480, fullScreen))
	{
		printfLog("Error while initializing the engine.\n");
		return 1;
	}

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
		return 1;
	}
#else
	return runTheGame(argc, pp_argv);
#endif
}