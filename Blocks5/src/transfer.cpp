#include "pch.h"
#include "transfer.h"
#include "filesystem.h"
#include "campaign.h"
#include "progressdb.h"
#include "util.h"

#ifdef __EMSCRIPTEN__
#include "web_transfer.h"
#elif defined(_WIN32)
#include "engine.h"
#include <commdlg.h>
#include <SDL_syswm.h>
#else
#include <cstdio>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#endif

namespace
{
	// Where the four folder kinds live, the same relative path under both
	// roots: the game folder for what ships, the user directory for the
	// player's own.
	std::string subdirectoryFor(Transfer::Kind kind)
	{
		switch(kind)
		{
		case Transfer::KIND_LEVEL:
		case Transfer::KIND_MUSIC:    return "levels/";
		case Transfer::KIND_CAMPAIGN: return "levels/campaigns/";
		case Transfer::KIND_SKIN:     return "levels/skins/";
		default:                      return "";
		}
	}

	// The kind's folder in the user directory. The progress database lies in
	// the user directory itself and is named outright: an empty subdirectory
	// would make list() read the game folder's root, where data.zip lies, and
	// point remove() at whatever it found there.
	std::string directoryFor(Transfer::Kind kind)
	{
		if(kind == Transfer::KIND_PROGRESS) return FileSystem::inst().getAppHomeDirectory();

		const std::string sub(subdirectoryFor(kind));
		if(sub.empty()) return "";
		return FileSystem::inst().getAppHomeDirectory() + sub;
	}

	const char* extensionFor(Transfer::Kind kind)
	{
		switch(kind)
		{
		case Transfer::KIND_LEVEL:    return ".xml";
		case Transfer::KIND_MUSIC:    return ".ogg";
		case Transfer::KIND_CAMPAIGN:
		case Transfer::KIND_SKIN:
		case Transfer::KIND_PROGRESS: return ".zip";
		default:                      return "";
		}
	}

	bool exportTo(Transfer::Kind kind, const std::string& name, const std::string& destPath)
	{
		// A plain copy, skins included: three of the four shipped skins are
		// packed with a password, and decrypting them on the way out would
		// bypass that protection. The recipient can still use one, since the
		// password rides along as password.txt and Level::getSkinFilename
		// reads it from there.
		FileSystem& fs = FileSystem::inst();
		// Over both roots, since what ships can be exported too. The progress
		// database ships nothing and has no subdirectory to resolve.
		const std::string source(kind == Transfer::KIND_PROGRESS
								 ? directoryFor(kind) + name
								 : fs.resolveContentPath(subdirectoryFor(kind) + name));
		if(!fs.fileExists(source)) return false;
		return fs.copyFile(source, destPath);
	}

	// The stem used when sanitizeFilenameStem() leaves nothing of the wanted
	// name.
	const char* defaultStemFor(Transfer::Kind kind)
	{
		switch(kind)
		{
		case Transfer::KIND_CAMPAIGN: return "campaign";
		case Transfer::KIND_MUSIC:    return "music";
		case Transfer::KIND_SKIN:     return "skin";
		default:                      return "level";
		}
	}

	// 64 MiB, the most an import takes on every platform. The largest thing
	// that comes in here is a campaign with music - the shipped one is
	// 8.3 MB, and this is room for a dozen full-length songs; classify()
	// reads a level whole, and a file picked by mistake can be a film. The
	// browser pays the most: a 63 MiB campaign held about 150 MiB more while
	// it came in and 90 MiB more after, since IDBFS keeps the home directory
	// in memory and the copy grows the wasm memory, which never shrinks.
	const uint MAX_IMPORT_SIZE = 64 * 1024 * 1024;
}

namespace Transfer
{

Kind classify(const std::string& path)
{
	FileSystem& fs = FileSystem::inst();

	// 1. Music: an Ogg page whose packet is Vorbis's identification header.
	//    "OggS" alone would take an Opus or a Theora file too, and the game
	//    decodes Vorbis only. The page header is 27 bytes and a segment table
	//    as long as its byte 26 says, and the packet follows.
	{
		File* p_file = fs.openFile(path, FileSystem::FM_READ);
		if(p_file)
		{
			unsigned char page[27 + 255 + 7];
			const uint got = p_file->read(page, sizeof(page));
			fs.closeFile(p_file);
			if(got >= 27 && !memcmp(page, "OggS", 4))
			{
				const uint packet = 27 + page[26];
				if(got >= packet + 7 && !memcmp(page + packet, "\x01vorbis", 7)) return KIND_MUSIC;
			}
		}
	}

	// 2. Archives. A ZIP's table of contents is not encrypted, so this works
	//    without the password. The path must end in ".zip", in any case, or
	//    FileSystem::convertPath does not see an archive in it.
	if(equalsNoCase(getFilenameExtension(path).c_str(), "zip"))
	{
		if(fs.fileExists(path + "/campaign.xml")) return KIND_CAMPAIGN;
		if(fs.fileExists(path + "/tileset.xml") &&
		   fs.fileExists(path + "/sprites.png")) return KIND_SKIN;
		if(ProgressDB::isProgressArchive(path)) return KIND_PROGRESS;
		return KIND_NONE;
	}

	// 3. Level. Parse only, no Level::load() - that would build objects and
	//    request textures merely to check a file.
	{
		TiXmlDocument doc;
		doc.SetCondenseWhiteSpace(false);
		doc.Parse(fs.readStringFromFile(path).c_str());
		if(!doc.Error() && doc.FirstChildElement("Level")) return KIND_LEVEL;
	}

	return KIND_NONE;
}

std::string targetName(Kind kind, const std::string& untrustedName)
{
	if(kind == KIND_NONE) return "";

	// The progress database has one name and one place, so the wish counts
	// for nothing: a second file beside it would never be read. For the other
	// four the wanted stem is reduced to [A-Za-z0-9_-] and given the kind's
	// extension.
	if(kind == KIND_PROGRESS) return FileSystem::inst().getPathFilename(ProgressDB::getFilename());

	return sanitizeFilenameStem(untrustedName, defaultStemFor(kind)) + extensionFor(kind);
}

bool wouldReplace(Kind kind, const std::string& untrustedName)
{
	const std::string name(targetName(kind, untrustedName));
	if(name.empty()) return false;

	// Only the user directory: that is where install() writes, and a name the
	// game folder holds is refused rather than replaced. For the progress
	// database the backup of an interrupted save counts too, or an import
	// right after one would replace it without asking.
	if(kind == KIND_PROGRESS) return ProgressDB::inst().exists();

	return FileSystem::inst().fileExists(directoryFor(kind) + name);
}

std::string install(Kind kind,
					const std::string& path,
					const std::string& untrustedName,
					std::string& errorId,
					bool* p_replaced)
{
	FileSystem& fs = FileSystem::inst();
	errorId = "";
	if(p_replaced) *p_replaced = false;

	// An existing file is replaced - a new version of your level means your
	// level and not a second one beside it. For a skin there is no other way:
	// its filename is its id, a level says skin0="space" and the loader looks
	// for levels/skins/space.zip.
	const std::string dir(directoryFor(kind));
	const std::string name(targetName(kind, untrustedName));

	// The one exception is a name the game ships something under: the game
	// folder wins, so a file of that name in the user directory could never
	// be loaded. isBuiltIn asks the game folder rather than a list, so it
	// covers whatever ships later.
	if(isBuiltIn(kind, name))
	{
		errorId = "$TR_ERROR_RESERVED";
		return "";
	}

	// The same question wouldReplace() answers, and for the progress database
	// that includes a backup left by an interrupted save.
	const bool replaced = (kind == KIND_PROGRESS) ? ProgressDB::inst().exists()
	                                              : fs.fileExists(dir + name);

	// A campaign must be loadable as well, not merely contain a campaign.xml,
	// and a progress database must parse. Both are checked before anything is
	// replaced, so a damaged file cannot destroy a good one of the same name.
	if((kind == KIND_CAMPAIGN && !Campaign::isImportableArchive(path)) ||
	   (kind == KIND_PROGRESS && !ProgressDB::canRead(path)))
	{
		errorId = "$TR_ERROR_BROKEN";
		return "";
	}

	// The progress database is put in place by its own class: a plain copy
	// truncates what is there, and it is the only kind with a backup that has
	// to be brought into the open first and cleared afterwards.
	const bool installed = (kind == KIND_PROGRESS)
						   ? ProgressDB::inst().installFrom(path)
						   : fs.copyFile(path, dir + name);
	if(!installed)
	{
		errorId = "$TR_ERROR_FAILED";
		return "";
	}

	if(p_replaced) *p_replaced = replaced;
	return name;
}

std::vector<std::string> list(Kind kind)
{
	std::vector<std::string> result;
	if(kind == KIND_NONE) return result;

	// Both roots, since both are playable. Nothing can be saved or imported
	// under a shipped name, so a name stands in both only for an example
	// level the player has saved a copy of (FileSystem::getPlayerFiles); the
	// find below lists it once.
	FileSystem& fs = FileSystem::inst();

	// The progress database is one file with one name, and only ever in the
	// user directory: there is nothing to list and nothing to sort.
	if(kind == KIND_PROGRESS)
	{
		// exists() and not fileExists(): right after an interrupted save the
		// whole database is standing under its backup's name, and an empty
		// list would tell the player their progress is gone.
		if(ProgressDB::inst().exists()) result.push_back(targetName(kind, ""));
		return result;
	}

	const std::string want(extensionFor(kind));
	const std::string sub(subdirectoryFor(kind));
	const std::string roots[] = { fs.getGameDirectory() + sub, fs.getAppHomeDirectory() + sub };

	for(uint r = 0; r < sizeof(roots) / sizeof(roots[0]); r++)
	{
		std::list<std::string> files = fs.listDirectory(roots[r]);
		for(std::list<std::string>::const_iterator i = files.begin(); i != files.end(); ++i)
		{
			if(std::string(".") + getFilenameExtension(*i) != want) continue;
			if(std::find(result.begin(), result.end(), *i) != result.end()) continue;
			result.push_back(*i);
		}
	}

	std::sort(result.begin(), result.end());
	return result;
}


bool isBuiltIn(Kind kind, const std::string& name)
{
	if(kind == KIND_NONE || name.empty()) return false;

	// The game ships no progress of its own, and never can: it is written by
	// playing.
	if(kind == KIND_PROGRESS) return false;
	const std::string sub(subdirectoryFor(kind));
	if(sub.empty()) return false;
	// Not a list but the disk: shipped is whatever lies in the game folder,
	// less the player's own files (FileSystem::getPlayerFiles). Case is the
	// file system's business, and rightly so: on Windows "Blocks.zip" is
	// "blocks.zip".
	return FileSystem::inst().isShippedContent(sub + name);
}

bool isRemovable(Kind kind, const std::string& name)
{
	if(kind == KIND_NONE || name.empty()) return false;
	if(isBuiltIn(kind, name)) return false;

	// Only ever the player's own version is deleted. For the two example
	// levels that version exists only once the player has saved one of them;
	// until then the entry is in the list but there is nothing to take away.
	FileSystem& fs = FileSystem::inst();
	return fs.fileExists(fs.getAppHomeDirectory() + subdirectoryFor(kind) + name);
}

bool remove(Kind kind, const std::string& name, std::string& errorId)
{
	errorId = "";

	// The button is switched off in these cases anyway; the guard belongs
	// here all the same and not in the GUI.
	if(kind == KIND_NONE || name.empty())
	{
		errorId = "$TR_ERROR_FAILED";
		return false;
	}
	if(!isRemovable(kind, name))
	{
		errorId = "$TR_ERROR_BUILT_IN";
		return false;
	}

	// The progress database takes its backup with it. One left behind would
	// be taken for an interrupted save by the next query and put back, which
	// would undo the deletion a moment after it appeared to work.
	const bool deleted = (kind == KIND_PROGRESS)
						 ? ProgressDB::inst().remove()
						 : FileSystem::inst().deleteFile(directoryFor(kind) + name);
	if(!deleted)
	{
		errorId = "$TR_ERROR_FAILED";
		return false;
	}

#ifdef __EMSCRIPTEN__
	// Write through to IndexedDB at once, as on import: otherwise the file
	// would only be deleted in memory and would be back after a reload.
	WebTransfer::syncHome();
#endif

	return true;
}

// ---------------------------------------------------------------------------
// The file dialog: three platforms, one interface.
// ---------------------------------------------------------------------------

#ifdef __EMSCRIPTEN__

namespace
{
	// C hands over all three possible targets and JS picks one of them by
	// extension, leaving C alone to compose every path. The staging file lies
	// outside the user directory, which keeps a rejected file out of the
	// IndexedDB entirely.
	const char* const p_stagingOgg = "/blocks5_import.ogg";
	const char* const p_stagingXml = "/blocks5_import.xml";
	const char* const p_stagingZip = "/blocks5_import.zip";

	std::string toLower(const std::string& text)
	{
		std::string result(text);
		for(size_t i = 0; i < result.length(); i++)
		{
			result[i] = static_cast<char>(tolower(static_cast<unsigned char>(result[i])));
		}
		return result;
	}

	std::string stagingFor(const std::string& untrustedName)
	{
		const std::string ext(toLower(getFilenameExtension(untrustedName)));
		if(ext == "ogg") return p_stagingOgg;
		if(ext == "xml") return p_stagingXml;
		if(ext == "zip") return p_stagingZip;
		return "";
	}

	std::string stagingPath;
}

bool beginImport()
{
	return WebTransfer::openPicker(p_stagingOgg, p_stagingXml, p_stagingZip, MAX_IMPORT_SIZE);
}

int pollImport(std::string& path, std::string& untrustedName)
{
	const int status = WebTransfer::pollImport(untrustedName);
	switch(status)
	{
	case WebTransfer::IMPORT_IDLE:      return STATUS_BUSY;
	case WebTransfer::IMPORT_CANCELLED: return STATUS_CANCELLED;
	case WebTransfer::IMPORT_TOO_BIG:   return STATUS_TOO_BIG;
	case WebTransfer::IMPORT_WRONG_TYPE: return STATUS_UNKNOWN;
	case WebTransfer::IMPORT_OK:        break;
	default:                            return STATUS_FAILED;
	}

	stagingPath = stagingFor(untrustedName);
	if(stagingPath.empty()) return STATUS_UNKNOWN;
	path = stagingPath;
	return STATUS_OK;
}

void finishImport()
{
	if(stagingPath.empty()) return;
	FileSystem::inst().deleteFile(stagingPath);
	stagingPath = "";
	// Write through to IndexedDB at once - otherwise the import would sit in
	// memory alone for up to five seconds.
	WebTransfer::syncHome();
}

void abandonImport()
{
	WebTransfer::abandon();
	FileSystem::inst().deleteFile(p_stagingOgg);
	FileSystem::inst().deleteFile(p_stagingXml);
	FileSystem::inst().deleteFile(p_stagingZip);
	stagingPath = "";
}

bool doExport(Kind kind, const std::string& name, std::string& errorId)
{
	errorId = "";
	const std::string tmp("/blocks5_export.tmp" + std::string(extensionFor(kind)));
	FileSystem::inst().deleteFile(tmp);
	if(!exportTo(kind, name, tmp))
	{
		FileSystem::inst().deleteFile(tmp);
		errorId = "$TR_ERROR_FAILED";
		return false;
	}
	// The name comes from our own directory and already carries its
	// extension - it serves unchanged as the suggestion for the download.
	WebTransfer::download(tmp, name);
	FileSystem::inst().deleteFile(tmp);
	return true;
}

#elif defined(_WIN32)

namespace
{
	std::string pickedPath;
	std::string pickedName;
	int  importStatus = STATUS_BUSY;
	bool wantDialog = false;

	// Just the base name, whatever separator built the path.
	std::string getFilenameFromPath(const std::string& path)
	{
		const size_t cut = path.find_last_of("/\\:");
		return cut == std::string::npos ? path : path.substr(cut + 1);
	}

	// Asked of the disk and not through File, which refuses a file its 32-bit
	// size cannot carry: the player is to hear that the file is too big, not
	// that it could not be read.
	bool isTooBig(const std::string& path)
	{
		WIN32_FILE_ATTRIBUTE_DATA data;
		if(!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &data)) return false;
		return data.nFileSizeHigh != 0 || data.nFileSizeLow > MAX_IMPORT_SIZE;
	}

	void buildFilter(char* p_buffer, size_t size)
	{
		// Doubly null-terminated list, the way the Common Dialog API wants it.
		static const char* p_parts[] =
		{
			"Blocks 5 (*.xml;*.zip;*.ogg)", "*.xml;*.zip;*.ogg",
			"All files (*.*)", "*.*"
		};
		size_t at = 0;
		for(uint i = 0; i < sizeof(p_parts) / sizeof(*p_parts); i++)
		{
			const size_t n = strlen(p_parts[i]) + 1;
			if(at + n + 1 > size) break;
			memcpy(p_buffer + at, p_parts[i], n);
			at += n;
		}
		if(at < size) p_buffer[at] = 0;
	}

	// The game's window. With no owner the dialog hangs off nothing, and
	// Windows then does not keep it above the game window.
	HWND gameWindow()
	{
		SDL_SysWMinfo info;
		SDL_VERSION(&info.version);
		if(SDL_GetWMInfo(&info) && info.window) return info.window;
		return GetActiveWindow();
	}

	// A file dialog runs a message loop of its own, and the main loop stands
	// still meanwhile; the window procedure keeps the picture fresh as while
	// the border is dragged (Engine::beginForeignMessageLoop). Owned by the
	// game window, the dialog stays above it, even above a borderless
	// fullscreen one.
	struct ModalScope
	{
		ModalScope()  { Engine::inst().beginForeignMessageLoop(); }
		~ModalScope()
		{
			Engine::inst().endForeignMessageLoop();
			// What the foreign loop let through is not a click by the player
			// on the game.
			Engine::inst().flushInput();
		}
	};
}

bool beginImport()
{
	// Only make a note. The dialog runs a round later in pollImport(), because
	// this call sits in the middle of the GUI's event dispatch - a modal
	// window would start a second message loop there.
	if(wantDialog) return false;
	wantDialog = true;
	return true;
}

int pollImport(std::string& path, std::string& untrustedName)
{
	if(wantDialog)
	{
		wantDialog = false;

		char filter[128] = "";
		buildFilter(filter, sizeof(filter));

		char file[MAX_PATH] = "";
		OPENFILENAMEA ofn;
		memset(&ofn, 0, sizeof(ofn));
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = gameWindow();
		ofn.lpstrFilter = filter;
		ofn.lpstrFile = file;
		ofn.nMaxFile = sizeof(file);
		// OFN_NOCHANGEDIR is mandatory: the game opens data.zip relative to the
		// working directory, and the dialog would otherwise change it.
		ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_HIDEREADONLY;

		ModalScope modal;
		if(GetOpenFileNameA(&ofn))
		{
			pickedPath = file;
			pickedName = getFilenameFromPath(pickedPath);
			importStatus = isTooBig(pickedPath) ? STATUS_TOO_BIG : STATUS_OK;
		}
		else importStatus = STATUS_CANCELLED;
	}

	const int status = importStatus;
	if(status == STATUS_BUSY) return STATUS_BUSY;
	importStatus = STATUS_BUSY;
	path = pickedPath;
	untrustedName = pickedName;
	return status;
}

void finishImport()
{
	// Under Windows nothing is staged - the user's file gets read where it
	// lies.
	pickedPath = "";
	pickedName = "";
}

void abandonImport()
{
	wantDialog = false;
	importStatus = STATUS_BUSY;
	finishImport();
}

bool doExport(Kind kind, const std::string& name, std::string& errorId)
{
	errorId = "";

	char filter[128] = "";
	buildFilter(filter, sizeof(filter));

	char file[MAX_PATH] = "";
	// The name comes from our own directory and already carries its
	// extension - it serves unchanged as the suggestion.
	strncpy(file, name.c_str(), sizeof(file) - 1);

	OPENFILENAMEA ofn;
	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = gameWindow();
	ofn.lpstrFilter = filter;
	ofn.lpstrFile = file;
	ofn.nMaxFile = sizeof(file);
	ofn.lpstrDefExt = extensionFor(kind) + 1;   // without the dot
	ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_HIDEREADONLY;

	{
		ModalScope modal;
		if(!GetSaveFileNameA(&ofn)) return false;   // cancelled, not an error
	}

	if(!exportTo(kind, name, file))
	{
		errorId = "$TR_ERROR_FAILED";
		return false;
	}
	return true;
}

#else

// ---------------------------------------------------------------------------
// Linux. Neither the standard library nor SDL 1.2 has a file dialog, so the
// desktop's own program runs it: zenity under GNOME, kdialog under KDE. Both
// print the chosen path and exit non-zero on a cancel, and the game links
// neither GTK nor Qt.
//
// The import runs alongside the game: pollImport() reads the dialog's pipe
// tick by tick, so the window keeps drawing. The export cannot, since
// doExport() returns its answer at once (transfer.h), and so it stops the
// game like the Windows dialog.
// ---------------------------------------------------------------------------

namespace
{
	std::string pickedPath;
	std::string pickedName;
	int   importStatus = STATUS_BUSY;
	bool  wantDialog = false;
	int   importFd = -1;
	pid_t importPid = -1;
	std::string importOutput;

	// Just the base name.
	std::string getFilenameFromPath(const std::string& path)
	{
		const size_t cut = path.find_last_of('/');
		return cut == std::string::npos ? path : path.substr(cut + 1);
	}

	// As under Windows, asked of the disk rather than through File.
	bool isTooBig(const std::string& path)
	{
		struct stat info;
		if(::stat(path.c_str(), &info) != 0) return false;
		return static_cast<unsigned long long>(info.st_size) > MAX_IMPORT_SIZE;
	}

	// Single quotes for a filename, which under Linux may hold almost any
	// character. Inside them only an apostrophe ends the string, so each one
	// closes the quotes, adds an escaped apostrophe and opens them again.
	std::string shellQuote(const std::string& text)
	{
		std::string quoted("'");
		for(size_t i = 0; i < text.length(); i++)
		{
			if(text[i] == '\'') quoted += "'\\''";
			else quoted += text[i];
		}
		return quoted + "'";
	}

	bool haveProgram(const char* p_name)
	{
		return ::system((std::string("command -v ") + p_name + " >/dev/null 2>&1").c_str()) == 0;
	}

	enum Dialog { DIALOG_NONE, DIALOG_ZENITY, DIALOG_KDIALOG };

	// Found once, remembered: otherwise the Manager asks the shell twice on
	// every click. Not finding one is not remembered, since the message asks
	// the player to install one and click again.
	Dialog findDialog()
	{
		static Dialog found = DIALOG_NONE;
		if(found == DIALOG_NONE)
		{
			if(haveProgram("zenity")) found = DIALOG_ZENITY;
			else if(haveProgram("kdialog")) found = DIALOG_KDIALOG;
			else printfLog("Neither zenity nor kdialog is installed - no file dialog available.\n");
		}
		return found;
	}

	std::string homeDir()
	{
		const char* p_home = ::getenv("HOME");
		return p_home && *p_home ? p_home : ".";
	}

	// The command line for one of the two dialogs, starting at a directory
	// for "open" and at the suggested file for "save as".
	std::string dialogCommand(Dialog dialog, bool save, const std::string& start)
	{
		if(dialog == DIALOG_ZENITY)
		{
			std::string command("zenity --file-selection");
			if(save) command += " --save --confirm-overwrite";
			command += " --filename=" + shellQuote(start);
			command += " --title=" + shellQuote(save ? "Blocks 5 - Export" : "Blocks 5 - Import");
			command += " --file-filter=" + shellQuote("Blocks 5 | *.xml *.zip *.ogg");
			command += " --file-filter=" + shellQuote("All files | *");
			return command + " 2>/dev/null";
		}

		std::string command("kdialog ");
		command += save ? "--getsavefilename " : "--getopenfilename ";
		command += shellQuote(start);
		command += " " + shellQuote("*.xml *.zip *.ogg|Blocks 5\n*|All files");
		return command + " 2>/dev/null";
	}

	// What the dialog program wrote, without the trailing line break.
	std::string trimmed(const std::string& text)
	{
		size_t end = text.length();
		while(end > 0 && (text[end - 1] == '\n' || text[end - 1] == '\r')) end--;
		return text.substr(0, end);
	}

	// Runs the dialog with its output on a pipe and keeps its process id.
	// popen() gives no id, and pclose() on a dialog still open waits for it,
	// so giving an import up on leaving the menu would stop the game until
	// the dialog closed. "exec" turns the shell into the dialog, so the id is
	// the dialog's own.
	//
	// The line is built before the fork: the game has threads, and between
	// fork() and exec() the child may only make async-signal-safe calls,
	// which an allocation is not.
	bool startDialog(const std::string& command)
	{
		const std::string line("exec " + command);
		int fds[2];
		if(::pipe(fds) != 0) return false;

		const pid_t pid = ::fork();
		if(pid < 0)
		{
			::close(fds[0]);
			::close(fds[1]);
			return false;
		}
		if(pid == 0)
		{
			::dup2(fds[1], STDOUT_FILENO);
			::close(fds[0]);
			::close(fds[1]);
			::execl("/bin/sh", "sh", "-c", line.c_str(), static_cast<char*>(0));
			::_exit(127);
		}

		::close(fds[1]);
		// Non-blocking, or pollImport()'s read() would stop the game until the
		// dialog closes.
		::fcntl(fds[0], F_SETFL, ::fcntl(fds[0], F_GETFL, 0) | O_NONBLOCK);
		importFd = fds[0];
		importPid = pid;
		return true;
	}

	// Closes the pipe and reaps the dialog; its exit code, or -1 where it did
	// not exit on its own. A dialog being given up is killed first, since one
	// still open would otherwise be waited for.
	int endDialog(bool giveUp)
	{
		if(giveUp) ::kill(importPid, SIGKILL);
		::close(importFd);
		int status = 0;
		while(::waitpid(importPid, &status, 0) < 0 && errno == EINTR) {}
		importFd = -1;
		importPid = -1;
		return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
	}
}

bool beginImport()
{
	// As under Windows, only make a note: this call sits in the middle of the
	// GUI's event dispatch. Without a dialog program the answer comes through
	// pollImport() too, since false would ask for a click that cannot help.
	if(wantDialog || importPid > 0) return false;
	if(findDialog() == DIALOG_NONE) importStatus = STATUS_NO_DIALOG;
	else wantDialog = true;
	return true;
}

int pollImport(std::string& path, std::string& untrustedName)
{
	if(wantDialog)
	{
		wantDialog = false;
		importOutput = "";
		if(!startDialog(dialogCommand(findDialog(), false, homeDir() + "/"))) importStatus = STATUS_FAILED;
	}

	if(importPid > 0)
	{
		char buffer[512];
		const ssize_t numBytesRead = ::read(importFd, buffer, sizeof(buffer));
		if(numBytesRead > 0) importOutput.append(buffer, numBytesRead);
		else if(numBytesRead == 0)
		{
			// End of the pipe: the dialog is closed, and its exit code says
			// whether the user cancelled.
			const int result = endDialog(false);
			pickedPath = trimmed(importOutput);
			pickedName = getFilenameFromPath(pickedPath);
			importStatus = (result == 0 && !pickedPath.empty()) ? STATUS_OK : STATUS_CANCELLED;
			if(importStatus == STATUS_OK && isTooBig(pickedPath)) importStatus = STATUS_TOO_BIG;
		}
		else if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
		{
			endDialog(true);
			importStatus = STATUS_FAILED;
		}
	}

	const int status = importStatus;
	if(status == STATUS_BUSY) return STATUS_BUSY;
	importStatus = STATUS_BUSY;
	path = pickedPath;
	untrustedName = pickedName;
	return status;
}

void finishImport()
{
	// As under Windows, nothing is staged - the user's file gets read where it
	// lies.
	pickedPath = "";
	pickedName = "";
}

void abandonImport()
{
	wantDialog = false;
	if(importPid > 0) endDialog(true);
	importStatus = STATUS_BUSY;
	finishImport();
}

bool doExport(Kind kind, const std::string& name, std::string& errorId)
{
	errorId = "";

	const Dialog dialog = findDialog();
	if(dialog == DIALOG_NONE)
	{
		errorId = "$TR_ERROR_NO_DIALOG";
		return false;
	}

	// The name comes from our own directory and already carries its
	// extension - it serves unchanged as the suggestion.
	const std::string extension(extensionFor(kind));
	std::string target(homeDir() + "/" + name);
	for(;;)
	{
		FILE* p_pipe = ::popen(dialogCommand(dialog, true, target).c_str(), "r");
		if(!p_pipe)
		{
			errorId = "$TR_ERROR_FAILED";
			return false;
		}

		std::string output;
		char buffer[512];
		size_t numBytesRead;
		while((numBytesRead = ::fread(buffer, 1, sizeof(buffer), p_pipe)) > 0) output.append(buffer, numBytesRead);
		const int result = ::pclose(p_pipe);

		target = trimmed(output);
		if(result != 0 || target.empty()) return false;   // cancelled, not an error

		// Neither kdialog nor zenity appends an extension. Without one the
		// import dialog's filter would hide the file, and classify() would not
		// take an archive for one at all: it knows a .zip only by the
		// extension. But the dialog asked before overwriting only the name it
		// was given, so a completed name that is taken goes back to it.
		if(target.length() >= extension.length() &&
		   equalsNoCase(target.c_str() + target.length() - extension.length(), extension.c_str())) break;
		target += extension;
		if(!FileSystem::inst().fileExists(target)) break;
	}

	if(!exportTo(kind, name, target))
	{
		errorId = "$TR_ERROR_FAILED";
		return false;
	}
	return true;
}

#endif

}
