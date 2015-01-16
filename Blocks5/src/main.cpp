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
#include "progressdb.h"
#include "stackwalker.h"

#ifdef _WIN32
#include <shellapi.h>
#include <wininet.h>
#endif

const char* p_localVersion = "1.1.2";

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
			HINTERNET inet = InternetOpenA("Scherfgen-Software Blocks 5", INTERNET_OPEN_TYPE_PRECONFIG, 0, 0, 0);
			if(!inet) return 1;

			HINTERNET url = InternetOpenUrlA(inet, "http://www.scherfgen-software.net/blocks5/version.txt", 0, 0, INTERNET_FLAG_RELOAD, 0);
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

	// Abfrage in einem Thread ausführen und maximal 2 Sekunden Zeit lassen
	Task task;
	DWORD threadID;
	HANDLE thread = CreateThread(0, 0, Task::threadProc, &task, 0, &threadID);
	WaitForSingleObject(thread, 2000);
	return task.finished ? task.currentVersion : "";
#else
#error NOT IMPLEMENTED
#endif
}

bool isNewer(const std::string& version1,
			 const std::string& version2)
{
	return version1 > version2;
}

const std::string detectInitializedVersion()
{
	// not_played:	kein "Blocks 5"-Ordner existiert im Benutzerverzeichnis und keine "progress.zip"-Datei existiert im Arbeitsverzeichnis
	// <= 1.0.7:	kein "Blocks 5"-Ordner existiert im Benutzerverzeichnis
	//    1.0.71:	"Blocks 5"-Ordner existiert im Benutzerverzeichnis
	//    1.0.72:	".initialized"-Datei existiert
	// >= 1.0.73:	".initialized"-Datei enthält die Versionsnummer

	FileSystem& fs = FileSystem::inst();
	const std::string homeDirectory(fs.getAppHomeDirectory());

	if(fs.listDirectory(homeDirectory).empty())
	{
		// TODO: Die "progress.zip" aus dem VirtualStore wird nicht gefunden! Warum nicht? Ging doch früher!
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
			// Verzeichnis initialisieren
			success &= fs.createDirectory(homeDirectory + "levels");
			success &= fs.createDirectory(homeDirectory + "levels/campaigns");
			success &= fs.createDirectory(homeDirectory + "levels/skins");
			success &= fs.createDirectory(homeDirectory + "screenshots");
			success &= fs.createDirectory(homeDirectory + "videos");
			if (fs.fileExists("config.xml")) success &= fs.copyFile("config.xml", homeDirectory + "config.xml");
			success &= fs.copyFile("videos/readme.txt", homeDirectory + "videos/readme.txt");
			if(versionInitialized == "<= 1.0.7") success &= fs.copyFile("progress.zip", homeDirectory + "progress.zip");
			success &= fs.copyFile("update_checker_disable.bat", homeDirectory + "update_checker_disable.bat");
			success &= fs.copyFile("update_checker_enable.bat", homeDirectory + "update_checker_enable.bat");
			if(fs.fileExists(".update_checker")) success &= fs.copyFile(".update_checker", homeDirectory + ".update_checker");

			std::list<std::string> fileList(fs.listDirectory("levels"));
			for(std::list<std::string>::const_iterator it = fileList.begin(); it != fileList.end(); ++it) success &= fs.copyFile(std::string("levels/") + *it, homeDirectory + "levels/" + *it);
			fileList = fs.listDirectory("levels/campaigns");
			for(std::list<std::string>::const_iterator it = fileList.begin(); it != fileList.end(); ++it) success &= fs.copyFile(std::string("levels/campaigns/") + *it, homeDirectory + "levels/campaigns/" + *it);
			fileList = fs.listDirectory("levels/skins");
			for(std::list<std::string>::const_iterator it = fileList.begin(); it != fileList.end(); ++it) success &= fs.copyFile(std::string("levels/skins/") + *it, homeDirectory + "levels/skins/" + *it);
			fileList = fs.listDirectory("screenshots");
			for(std::list<std::string>::const_iterator it = fileList.begin(); it != fileList.end(); ++it) success &= fs.copyFile(std::string("screenshots/") + *it, homeDirectory + "screenshots/" + *it);

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
				}
#endif
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
			success &= fs.copyFile("update_checker_disable.bat", homeDirectory + "update_checker_disable.bat");
			success &= fs.copyFile("update_checker_enable.bat", homeDirectory + "update_checker_enable.bat");
			if(fs.fileExists(".update_checker")) success &= fs.copyFile(".update_checker", homeDirectory + ".update_checker");

			success &= fs.createDirectory(homeDirectory + "videos");
			success &= fs.copyFile("videos/readme.txt", homeDirectory + "videos/readme.txt");

			if(!success) errorMsg = "Could not migrate all settings!";
		}
		else if(versionInitialized == "1.0.73")
		{
			fs.deleteFile(homeDirectory + "updates.no");
			success &= fs.copyFile("update_checker_disable.bat", homeDirectory + "update_checker_disable.bat");
			success &= fs.copyFile("update_checker_enable.bat", homeDirectory + "update_checker_enable.bat");
			if (fs.fileExists(".update_checker")) success &= fs.copyFile(".update_checker", homeDirectory + ".update_checker");

			if(!success) errorMsg = "Could not migrate all settings!";
		}

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

	if (!fs.fileExists(homeDirectory + ".update_checker")) fs.writeStringToFile("0", homeDirectory + ".update_checker");
	const std::string updateCheckerStatus(fs.readStringFromFile(homeDirectory + ".update_checker"));
	if(!updateCheckerStatus.empty() && updateCheckerStatus[0] == '1')
	{
		printfLog("Checking for update ...\n");

		// Neue Version da?
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
				// Seite öffnen
				ShellExecuteA(0, "open", "Blocks 5 Website.url", NULL, NULL, SW_SHOWNORMAL);
				return 0;
			}
		}
#else
#error NOT IMPLEMENTED
#endif
	}
	else
	{
		// Keine automatischen Updates!
		printfLog("Not checking for update!\n");
	}

	// Daten aus dem verschlüsselten Archiv lesen
	fs.pushCurrentDir("data.zip[3Cs18Ab0bV0Aat3Wf27le1ZM12kt0Xs05Aa4PX1EyI2V112Jr26v2GZO3dN0Ec91hk024P3cA32bc3GZ07Em4bf34st4320F7d13S00wd4Mg1ANn4SF2EO94Hz13Qq0LO18iY4Qy2C8r2XF28Bh]");
	
	// Alternativ: Daten aus dem lokalen Verzeichnis lesen
	// fs.pushCurrentDir("data");

	// Fortschritt laden
	ProgressDB::inst().load();

	bool fullScreen;
	bool useHQ2X = false;

#ifdef _DEBUG
	fullScreen = false;
#else
	fullScreen = true;
#endif

	// Argumente parsen
	for(int i = 0; i < argc; i++)
	{
		char* p_arg = pp_argv[i];
		if(!_stricmp(p_arg, "-windowed")) fullScreen = false;
		else if(!_stricmp(p_arg, "-fullScreen")) fullScreen = true;
		else if(!_stricmp(p_arg, "-hq2x")) useHQ2X = true;
	}

	printfLog("Initializing engine ...\n");
	Engine& engine = Engine::inst();

	// Spielaktionen festlegen
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

	// Engine-Aktionen festlegen
	p_action = engine.registerAction("$A_TOGGLE_MUTE", engine.getKeyboardVK(SDLK_F1));
	p_action->delay = INT_MAX;
	p_action = engine.registerAction("$A_CAPTURE_SCREENSHOT", engine.getKeyboardVK(SDLK_F11));
	p_action->delay = INT_MAX;
	p_action = engine.registerAction("$A_TOGGLE_CAPTURE_VIDEO", engine.getKeyboardVK(SDLK_F12));
	p_action->delay = INT_MAX;

	if(!engine.init("Blocks 5", "window.png", 640, 480, fullScreen, useHQ2X))
	{
		printfLog("Error while initializing the engine.\n");
		return 1;
	}

	// Lokalisierung laden
	engine.loadStringDB("languages.txt");

	// Instanzen der Spielzustandsklassen erzeugen
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
	// TODO: http://blog.kalmbachnet.de/?postid=75 beachten (StackWalker-Homepage: http://stackwalker.codeplex.com/releases/view/35258)

#ifdef _DEBUG
	return runTheGame(argc, pp_argv);
#else
	__try
	{
		return runTheGame(argc, pp_argv);
	}
	__except(expFilter(GetExceptionInformation(), GetExceptionCode()))
	{
		return 1;
	}
#endif
}