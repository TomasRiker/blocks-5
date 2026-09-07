#include "pch.h"
#include "transfer.h"
#include "filesystem.h"
#include "file.h"
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
#endif

namespace
{
	// Where the four kinds live - the same path twice: once under the user
	// directory, where the game reads them, and once relative to the working
	// directory, where the shipped ones sit.
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

	// The progress database is the one kind with no folder of its own: it
	// lies in the user directory itself. It is named here rather than given
	// an empty subdirectory, because an empty one would make list() read the
	// game folder's own root - where data.zip lies - and point remove() at
	// whatever it found there.
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
		// A plain copy, nothing else. For skins too, and there above all: three
		// of the four shipped ones are packed with a password, and decrypting
		// them on the way out would be a back door around the very protection
		// they are packed for. The recipient can still use the archive - the
		// password rides along inside it as password.txt, and
		// Level::getSkinFilename reads it out there.
		FileSystem& fs = FileSystem::inst();
		// Over both roots, because the export can be run on what the game
		// ships as well - except for the progress database, of which the game
		// ships nothing and which has no subdirectory to resolve.
		const std::string source(kind == Transfer::KIND_PROGRESS
								 ? directoryFor(kind) + name
								 : fs.resolveContentPath(subdirectoryFor(kind) + name));
		if(!fs.fileExists(source)) return false;
		return fs.copyFile(source, destPath);
	}

	// The name used when nothing is left of the wanted one - because it
	// consists of nothing but characters sanitizeFilenameStem() does not let
	// through, say.
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
}

namespace Transfer
{

Kind classify(const std::string& path)
{
	FileSystem& fs = FileSystem::inst();

	// 1. Music. Every Ogg page begins with "OggS", the first one included.
	//    That is cheaper and more honest than looking at the file extension.
	{
		File* p_file = fs.openFile(path, FileSystem::FM_READ);
		if(p_file)
		{
			char magic[4] = { 0, 0, 0, 0 };
			const uint got = p_file->read(magic, 4);
			fs.closeFile(p_file);
			if(got == 4 && !memcmp(magic, "OggS", 4)) return KIND_MUSIC;
		}
	}

	// 2. Archives. Looking inside works even for encrypted members without
	//    the password, because a ZIP's table of contents lies open. This
	//    check requires path to end in ".zip", or FileSystem::convertPath
	//    does not recognise the archive.
	if(getFilenameExtension(path) == "zip")
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
	// for nothing: a second one beside it would be a file the game never
	// reads. For the other four the wanted name is cut down to [A-Za-z0-9_-]
	// and given the kind's extension.
	if(kind == KIND_PROGRESS) return FileSystem::inst().getPathFilename(ProgressDB::getFilename());

	return sanitizeFilenameStem(untrustedName, defaultStemFor(kind)) + extensionFor(kind);
}

bool wouldReplace(Kind kind, const std::string& untrustedName)
{
	const std::string name(targetName(kind, untrustedName));
	if(name.empty()) return false;

	// The user directory and not both roots: that is where install() writes,
	// and a name the game folder holds is refused outright rather than
	// replaced. For the progress database either name counts, or an import
	// right after an interrupted save would replace it without asking.
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

	// The one exception: the seven names under which the game itself ships
	// something. Overwriting one would take something from the player that
	// they do not get back.
	if(isBuiltIn(kind, name))
	{
		errorId = "$TR_ERROR_RESERVED";
		return "";
	}

	const bool replaced = fs.fileExists(dir + name);

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

	// Both roots together, since both are playable: what the game ships sits
	// in the game folder, what the player made or imported in the user
	// directory. A name can occur only once - nothing can be saved or
	// imported under a shipped name - hence the union needs no rule of its
	// own for that, and the comparison further down catches the double insert.
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
	// Not a list but the disk: shipped is whatever lies in the game folder.
	// On Windows "Blocks.zip" is the same file as "blocks.zip", and because
	// the file system there does not tell them apart, fileExists() answers
	// yes to the differently spelled name too - which is exactly right here.
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
// The file dialog. Two worlds, one interface.
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
	// 48 MiB. The largest thing that comes in here is a campaign with music;
	// the shipped one is 8.3 MB.
	return WebTransfer::openPicker(p_stagingOgg, p_stagingXml, p_stagingZip, 50331648u);
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

	// A file dialog brings a foreign message loop with it: the game's main
	// loop stands still while it is open. The window stays visible because
	// the window procedure keeps drawing meanwhile - the same machinery as
	// when the window border is dragged. The dialog belongs to the game
	// window (hwndOwner), and Windows always keeps a window with an owner
	// above it, even above a borderless fullscreen window.
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
			importStatus = STATUS_OK;
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
// Linux. There is no file dialog in the standard library and none in SDL 1.2;
// every desktop environment instead ships a small program that does exactly
// that - zenity under GNOME, kdialog under KDE. Both write the chosen path to
// stdout and return a non-zero exit code on a cancelled dialog. The game
// therefore has to link neither GTK nor Qt.
//
// The import runs alongside: popen() gives a pipe that pollImport() reads tick
// by tick, keeping the window drawing while the dialog is open. The export
// cannot do that - doExport() delivers its result at once, which is how
// transfer.h declares it - and therefore stops the game like the modal dialog
// under Windows.
// ---------------------------------------------------------------------------

namespace
{
	std::string pickedPath;
	std::string pickedName;
	int   importStatus = STATUS_BUSY;
	bool  wantDialog = false;
	FILE* p_importPipe = 0;
	std::string importOutput;

	// Just the base name.
	std::string getFilenameFromPath(const std::string& path)
	{
		const size_t cut = path.find_last_of('/');
		return cut == std::string::npos ? path : path.substr(cut + 1);
	}

	// Everything that goes into a command line here is a filename - and
	// under Linux a filename may contain almost any character, the
	// apostrophe included. Inside single quotes an apostrophe is the only
	// thing that ends the string; leaving them for it and entering them
	// again afterwards is the usual answer.
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

	// Search once and remember: otherwise the Manager asks the shell twice
	// on every click.
	Dialog findDialog()
	{
		static Dialog found = DIALOG_NONE;
		static bool searched = false;
		if(!searched)
		{
			searched = true;
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

	// The command line for one of the two dialogs. An empty suggestion means
	// "open", otherwise "save as".
	std::string dialogCommand(Dialog dialog, const std::string& suggestion)
	{
		const bool save = !suggestion.empty();
		if(dialog == DIALOG_ZENITY)
		{
			std::string command("zenity --file-selection");
			if(save) command += " --save --confirm-overwrite --filename=" + shellQuote(homeDir() + "/" + suggestion);
			else     command += " --filename=" + shellQuote(homeDir() + "/");
			command += " --title=" + shellQuote(save ? "Blocks 5 - Export" : "Blocks 5 - Import");
			command += " --file-filter=" + shellQuote("Blocks 5 | *.xml *.zip *.ogg");
			command += " --file-filter=" + shellQuote("All files | *");
			return command + " 2>/dev/null";
		}

		std::string command("kdialog ");
		command += save ? "--getsavefilename " : "--getopenfilename ";
		command += shellQuote(homeDir() + "/" + suggestion);
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
}

bool beginImport()
{
	// As under Windows, only make a note: this call sits in the middle of the
	// GUI's event dispatch.
	if(wantDialog || p_importPipe) return false;
	if(findDialog() == DIALOG_NONE) return false;
	wantDialog = true;
	return true;
}

int pollImport(std::string& path, std::string& untrustedName)
{
	if(wantDialog)
	{
		wantDialog = false;
		importOutput = "";
		p_importPipe = ::popen(dialogCommand(findDialog(), "").c_str(), "r");
		if(!p_importPipe) importStatus = STATUS_FAILED;
		else
		{
			// Without O_NONBLOCK the game would stand still in read() until
			// the user closes the dialog - that is exactly what it must not do.
			const int fd = ::fileno(p_importPipe);
			::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
		}
	}

	if(p_importPipe)
	{
		char buffer[512];
		const ssize_t numBytesRead = ::read(::fileno(p_importPipe), buffer, sizeof(buffer));
		if(numBytesRead > 0) importOutput.append(buffer, numBytesRead);
		else if(numBytesRead == 0)
		{
			// End of the pipe: the dialog is closed. pclose() delivers the
			// exit code, and that says whether the user cancelled.
			const int result = ::pclose(p_importPipe);
			p_importPipe = 0;
			pickedPath = trimmed(importOutput);
			pickedName = getFilenameFromPath(pickedPath);
			importStatus = (result == 0 && !pickedPath.empty()) ? STATUS_OK : STATUS_CANCELLED;
		}
		else if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
		{
			::pclose(p_importPipe);
			p_importPipe = 0;
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
	if(p_importPipe)
	{
		::pclose(p_importPipe);
		p_importPipe = 0;
	}
	importStatus = STATUS_BUSY;
	finishImport();
}

bool doExport(Kind kind, const std::string& name, std::string& errorId)
{
	errorId = "";

	const Dialog dialog = findDialog();
	if(dialog == DIALOG_NONE)
	{
		errorId = "$TR_ERROR_FAILED";
		return false;
	}

	// The name comes from our own directory and already carries its
	// extension - it serves unchanged as the suggestion.
	FILE* p_pipe = ::popen(dialogCommand(dialog, name).c_str(), "r");
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

	std::string target(trimmed(output));
	if(result != 0 || target.empty()) return false;   // cancelled, not an error

	// Neither kdialog nor zenity appends an extension. Without one the file
	// could not be read back in later - classify() does look inside the file,
	// but the import dialog filters by extension.
	const std::string extension(extensionFor(kind));
	if(target.length() < extension.length() ||
	   target.compare(target.length() - extension.length(), extension.length(), extension) != 0)
	{
		target += extension;
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
