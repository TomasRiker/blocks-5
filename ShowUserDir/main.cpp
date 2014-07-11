#include <string>
#include <Windows.h>
#include <Shlobj.h>

int WINAPI WinMain(HINSTANCE inst,
				   HINSTANCE prevInst,
				   char* p_cmdLine,
				   int showCmd)
{
	char path[256];
	SHGetFolderPathA(NULL, CSIDL_MYDOCUMENTS, 0, 0, path);
	const std::string homeDirectory(std::string(path) + "\\Blocks 5");

	if(GetFileAttributesA(homeDirectory.c_str()) == INVALID_FILE_ATTRIBUTES)
	{
		// Das Verzeichnis existiert noch nicht!
		MessageBoxA(0, "The user directory has not been created, because the game has not been started yet. Start the game, and the directory will be created and initialized.",
					"Start the game first!", MB_OK | MB_ICONINFORMATION);
	}
	else
	{
		// Explorer öffnen
		HINSTANCE result = ShellExecuteA(0, "explore", homeDirectory.c_str(), 0, 0, SW_SHOWNORMAL);
		if(reinterpret_cast<int>(result) <= 32)
		{
			const std::string errorMsg(std::string("Could not open Windows Explorer for directory:\r\n\"") + homeDirectory + "\"");
			MessageBoxA(0, errorMsg.c_str(), "Error!", MB_OK | MB_ICONERROR);
			return 1;
		}
	}

	return 0;
}