#include <string>
#include <Windows.h>
#include <Shlobj.h>

int WINAPI WinMain(HINSTANCE inst,
				   HINSTANCE prevInst,
				   char* p_cmdLine,
				   int showCmd)
{
	// Where the game puts it (FileSystem::getAppHomeDirectory): in the
	// Documents folder, and without one beside the game, which is where this
	// program lies as well. S_OK and not SUCCEEDED: the ANSI call answers a
	// folder that does not exist with S_FALSE, a success code. MAX_PATH is
	// the size both calls are specified for.
	char path[MAX_PATH];
	std::string homeDirectory;
	if(SHGetFolderPathA(NULL, CSIDL_MYDOCUMENTS, 0, 0, path) == S_OK) homeDirectory = std::string(path) + "\\Blocks 5";
	else
	{
		const DWORD length = GetModuleFileNameA(0, path, MAX_PATH);
		if(length == 0 || length >= MAX_PATH)
		{
			MessageBoxA(0, "Windows could not say where the Documents folder is.", "Error!", MB_OK | MB_ICONERROR);
			return 1;
		}
		const std::string exe(path, length);
		homeDirectory = exe.substr(0, exe.find_last_of("\\/") + 1) + "Blocks 5";
	}

	if(GetFileAttributesA(homeDirectory.c_str()) == INVALID_FILE_ATTRIBUTES)
	{
		// The directory does not exist yet!
		MessageBoxA(0, "The user directory has not been created, because the game has not been started yet. Start the game, and the directory will be created and initialized.",
					"Start the game first!", MB_OK | MB_ICONINFORMATION);
	}
	else
	{
		// open Explorer
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