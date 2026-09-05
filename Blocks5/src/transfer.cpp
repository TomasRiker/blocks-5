#include "pch.h"
#include "transfer.h"
#include "filesystem.h"
#include "file.h"
#include "campaign.h"
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
	// Wo die vier Arten liegen - derselbe Pfad zweimal: einmal unter dem
	// Benutzerverzeichnis, wo das Spiel sie liest, und einmal relativ zum
	// Arbeitsverzeichnis, wo die mitgelieferten stehen.
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

	std::string directoryFor(Transfer::Kind kind)
	{
		const std::string sub(subdirectoryFor(kind));
		if(sub.empty()) return "";
		return FileSystem::inst().getAppHomeDirectory() + sub;
	}

	// Was mitgeliefert wird: genau das, was nach einer Neuinstallation im
	// Benutzerverzeichnis liegt. zip_skins.bat baut die vier Skins, die
	// Kampagne heisst blocks.zip, und die beiden Beispiellevel liegen lose
	// daneben. Die eine Liste, die isBuiltIn() und refreshBuiltIns() teilen.
	const char* const* builtInNames(Transfer::Kind kind)
	{
		static const char* p_levels[]    = { "example01.xml", "example02.xml", 0 };
		static const char* p_campaigns[] = { "blocks.zip", 0 };
		static const char* p_skins[]     = { "blocks_01.zip", "blocks_02.zip",
											 "blocks_03.zip", "space.zip", 0 };

		switch(kind)
		{
		case Transfer::KIND_LEVEL:    return p_levels;
		case Transfer::KIND_CAMPAIGN: return p_campaigns;
		case Transfer::KIND_SKIN:     return p_skins;
		default:                      return 0;
		}
	}

	// Die Groesse einer Datei, oder 0, wenn es sie nicht gibt. Reicht als
	// Vergleich: die mitgelieferten Dateien kann niemand ersetzen, ein
	// Unterschied kann also nur daher kommen, dass die eine aelter ist.
	uint sizeOf(const std::string& path)
	{
		FileSystem& fs = FileSystem::inst();
		File* p_file = fs.openFile(path, FileSystem::FM_READ);
		if(!p_file) return 0;
		const uint size = p_file->getSize();
		fs.closeFile(p_file);
		return size;
	}

	const char* extensionFor(Transfer::Kind kind)
	{
		switch(kind)
		{
		case Transfer::KIND_LEVEL:    return ".xml";
		case Transfer::KIND_MUSIC:    return ".ogg";
		case Transfer::KIND_CAMPAIGN:
		case Transfer::KIND_SKIN:     return ".zip";
		default:                      return "";
		}
	}

	bool exportTo(Transfer::Kind kind, const std::string& name, const std::string& destPath)
	{
		// Eine Kopie, sonst nichts. Auch beim Skin, und gerade dort: drei der vier
		// mitgelieferten sind mit einem Passwort gepackt, und sie beim Hinausgehen
		// zu entschluesseln waere eine Hintertuer um genau diesen Schutz herum.
		// Benutzen kann der Empfaenger das Archiv trotzdem - das Passwort liegt als
		// password.txt darin, und Level::getSkinFilename liest es dort aus.
		FileSystem& fs = FileSystem::inst();
		const std::string source(directoryFor(kind) + name);
		if(!fs.fileExists(source)) return false;
		return fs.copyFile(source, destPath);
	}

	// Der Name, wenn vom Wunschnamen nichts uebrig bleibt - etwa weil er aus
	// lauter Zeichen besteht, die sanitizeFilenameStem() nicht durchlaesst.
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

	// 1. Musik. Jede Ogg-Seite beginnt mit "OggS", die erste also auch. Das
	//    ist billiger und ehrlicher als der Blick auf die Dateiendung.
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

	// 2. Archive. Nachsehen geht auch bei verschluesselten Mitgliedern ohne
	//    Passwort, weil das Inhaltsverzeichnis eines ZIP offen liegt. Diese
	//    Pruefung setzt voraus, dass path auf ".zip" endet - sonst erkennt
	//    FileSystem::convertPath das Archiv nicht.
	if(getFilenameExtension(path) == "zip")
	{
		if(fs.fileExists(path + "/campaign.xml")) return KIND_CAMPAIGN;
		if(fs.fileExists(path + "/tileset.xml") &&
		   fs.fileExists(path + "/sprites.png")) return KIND_SKIN;
		return KIND_NONE;
	}

	// 3. Level. Nur parsen, keine Level::load() - das wuerde Objekte bauen
	//    und Texturen anfordern, nur um eine Datei zu pruefen.
	{
		TiXmlDocument doc;
		doc.SetCondenseWhiteSpace(false);
		doc.Parse(fs.readStringFromFile(path).c_str());
		if(!doc.Error() && doc.FirstChildElement("Level")) return KIND_LEVEL;
	}

	return KIND_NONE;
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

	// Fuer alle vier Arten dasselbe: der Wunschname, auf [A-Za-z0-9_-]
	// zusammengestrichen, plus die Endung der Art. Gibt es die Datei schon,
	// wird sie ersetzt - wer eine neue Fassung seines Levels einspielt, meint
	// seinen Level und nicht einen zweiten daneben. Beim Skin ginge es gar
	// nicht anders: sein Dateiname ist seine Kennung, ein Level sagt
	// skin0="space" und der Lader sucht levels/skins/space.zip.
	const std::string dir(directoryFor(kind));
	const std::string name(sanitizeFilenameStem(untrustedName, defaultStemFor(kind)) +
						   extensionFor(kind));

	// Die eine Ausnahme: die sieben Namen, unter denen das Spiel selbst etwas
	// mitliefert. Ueberschreiben hiesse hier, dem Spieler etwas wegzunehmen,
	// das er nicht wiederbekommt.
	if(isBuiltIn(kind, name))
	{
		errorId = "$TR_ERROR_RESERVED";
		return "";
	}

	const bool replaced = fs.fileExists(dir + name);

	// Eine Kampagne muss sich auch laden lassen, nicht nur eine campaign.xml
	// enthalten. Das wird geprueft, bevor irgendetwas ersetzt wird.
	if(kind == KIND_CAMPAIGN && !Campaign::isImportableArchive(path))
	{
		errorId = "$TR_ERROR_BROKEN";
		return "";
	}

	if(!fs.copyFile(path, dir + name))
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

	const std::string want(extensionFor(kind));
	std::list<std::string> files = FileSystem::inst().listDirectory(directoryFor(kind));
	for(std::list<std::string>::const_iterator i = files.begin(); i != files.end(); ++i)
	{
		if(std::string(".") + getFilenameExtension(*i) != want) continue;
		result.push_back(*i);
	}

	std::sort(result.begin(), result.end());
	return result;
}

bool refreshBuiltIns()
{
	// Verglichen wird die Groesse und nicht die Version: der Ordner soll sich
	// auch dann fangen, wenn sich an einem Skin etwas geaendert hat, ohne dass
	// die Versionsnummer weitergerueckt ist. Ueberschrieben wird nur, was hier
	// als mitgeliefert steht - und genau diese Namen kann der Spieler weder
	// importieren noch loeschen, es sind also nie seine Dateien.
	FileSystem& fs = FileSystem::inst();
	const Kind kinds[] = { KIND_LEVEL, KIND_CAMPAIGN, KIND_SKIN };
	bool ok = true;

	for(uint k = 0; k < sizeof(kinds) / sizeof(kinds[0]); k++)
	{
		const std::string sub(subdirectoryFor(kinds[k]));
		const std::string home(directoryFor(kinds[k]));

		for(const char* const* pp_name = builtInNames(kinds[k]); pp_name && *pp_name; pp_name++)
		{
			const std::string source(sub + *pp_name);
			const std::string target(home + *pp_name);

			// Fehlt die Vorlage, ist das Spiel unvollstaendig ausgepackt. Das
			// faellt an anderer Stelle laut genug auf; hier bleibt die
			// vorhandene Kopie besser stehen, als sie zu loeschen.
			const uint sourceSize = sizeOf(source);
			if(!sourceSize)
			{
				printfLog("+ WARNING: Shipped file \"%s\" is missing.\n", source.c_str());
				continue;
			}

			if(sizeOf(target) == sourceSize) continue;

			printfLog("* Refreshing \"%s\" in the user directory.\n", *pp_name);
			if(!fs.copyFile(source, target))
			{
				printfLog("+ ERROR: Could not refresh \"%s\".\n", target.c_str());
				ok = false;
			}
		}
	}

	return ok;
}

bool isBuiltIn(Kind kind, const std::string& name)
{
	const char* const* pp_names = builtInNames(kind);
	if(!pp_names) return false;

	// Ohne Ruecksicht auf Gross- und Kleinschreibung: unter Windows ist
	// "Blocks.zip" dieselbe Datei wie "blocks.zip", und ein nur anders
	// geschriebener Name ginge sonst hier vorbei.
	for(; *pp_names; pp_names++)
	{
		if(equalsNoCase(name.c_str(), *pp_names)) return true;
	}
	return false;
}

bool remove(Kind kind, const std::string& name, std::string& errorId)
{
	errorId = "";

	// Der Knopf ist in diesen Faellen ohnehin abgeschaltet; die Sperre gehoert
	// trotzdem hierher und nicht in die Oberflaeche.
	if(kind == KIND_NONE || name.empty())
	{
		errorId = "$TR_ERROR_FAILED";
		return false;
	}
	if(isBuiltIn(kind, name))
	{
		errorId = "$TR_ERROR_BUILT_IN";
		return false;
	}

	if(!FileSystem::inst().deleteFile(directoryFor(kind) + name))
	{
		errorId = "$TR_ERROR_FAILED";
		return false;
	}

#ifdef __EMSCRIPTEN__
	// Sofort nach IndexedDB durchschreiben, wie beim Import: sonst waere die
	// Datei nur im Arbeitsspeicher geloescht und nach einem Neuladen wieder da.
	WebTransfer::syncHome();
#endif

	return true;
}

// ---------------------------------------------------------------------------
// Der Dateidialog. Zwei Welten, eine Schnittstelle.
// ---------------------------------------------------------------------------

#ifdef __EMSCRIPTEN__

namespace
{
	// C gibt alle drei moeglichen Ziele vor und JS sucht sich nach der Endung
	// eines davon aus, damit weiterhin niemand ausser C einen Pfad zusammensetzt.
	// Die Zwischendatei liegt ausserhalb des Benutzerverzeichnisses, damit eine
	// abgelehnte Datei gar nicht erst in die IndexedDB kommt.
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
	// 48 MiB. Die groesste Sache, die hier hereinkommt, ist eine Kampagne mit
	// Musik; die mitgelieferte hat 8,3 MB.
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
	// Sofort nach IndexedDB durchschreiben - sonst stuende der Import bis zu
	// fuenf Sekunden lang nur im Arbeitsspeicher.
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
	// Der Name kommt aus dem eigenen Verzeichnis und traegt seine Endung
	// schon - er taugt unveraendert als Vorschlag fuer den Download.
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

	// Nur der Basisname, egal mit welchem Trenner der Pfad gebaut war.
	std::string getFilenameFromPath(const std::string& path)
	{
		const size_t cut = path.find_last_of("/\\:");
		return cut == std::string::npos ? path : path.substr(cut + 1);
	}

	void buildFilter(char* p_buffer, size_t size)
	{
		// Doppelt nullterminierte Liste, so will es die Common Dialog API.
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

	// Das Fenster des Spiels. Ohne Besitzer haengt der Dialog an nichts, und
	// Windows haelt ihn dann nicht ueber dem Spielfenster.
	HWND gameWindow()
	{
		SDL_SysWMinfo info;
		SDL_VERSION(&info.version);
		if(SDL_GetWMInfo(&info) && info.window) return info.window;
		return GetActiveWindow();
	}

	// Ein Dateidialog bringt eine fremde Nachrichtenschleife mit: die
	// Hauptschleife des Spiels steht, solange er offen ist. Das Fenster bleibt
	// sichtbar, weil die Fensterprozedur waehrenddessen weiterzeichnet -
	// dieselbe Vorrichtung wie beim Ziehen am Fensterrand. Der Dialog gehoert
	// dem Spielfenster (hwndOwner), und ein Fenster mit Besitzer haelt Windows
	// immer darueber, auch ueber einem randlosen Vollbildfenster.
	struct ModalScope
	{
		ModalScope()  { Engine::inst().beginForeignMessageLoop(); }
		~ModalScope()
		{
			Engine::inst().endForeignMessageLoop();
			// Was die fremde Schleife durchgelassen hat, ist kein Klick des
			// Spielers auf das Spiel.
			Engine::inst().flushInput();
		}
	};
}

bool beginImport()
{
	// Nur vormerken. Der Dialog laeuft eine Runde spaeter in pollImport(), denn
	// hier stecken wir mitten in der Ereignisverteilung der GUI - ein modales
	// Fenster startete dort eine zweite Nachrichtenschleife.
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
		// OFN_NOCHANGEDIR ist Pflicht: das Spiel oeffnet data.zip relativ zum
		// Arbeitsverzeichnis, und der Dialog wuerde es sonst verstellen.
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
	// Unter Windows wurde nichts zwischengelagert - die Datei des Benutzers
	// wurde gelesen, wo sie lag.
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
	// Der Name kommt aus dem eigenen Verzeichnis und traegt seine Endung
	// schon - er taugt unveraendert als Vorschlag.
	strncpy(file, name.c_str(), sizeof(file) - 1);

	OPENFILENAMEA ofn;
	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = gameWindow();
	ofn.lpstrFilter = filter;
	ofn.lpstrFile = file;
	ofn.nMaxFile = sizeof(file);
	ofn.lpstrDefExt = extensionFor(kind) + 1;   // ohne den Punkt
	ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_HIDEREADONLY;

	{
		ModalScope modal;
		if(!GetSaveFileNameA(&ofn)) return false;   // abgebrochen, kein Fehler
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
// Linux. Es gibt keinen Dateidialog in der Standardbibliothek und keinen in
// SDL 1.2; jede Arbeitsumgebung bringt statt dessen ein kleines Programm mit,
// das genau das tut - zenity unter GNOME, kdialog unter KDE. Beide schreiben
// den gewaehlten Pfad nach stdout und liefern einen Rueckgabewert ungleich
// null, wenn abgebrochen wurde. Damit braucht das Spiel weder GTK noch Qt zu
// binden.
//
// Der Import laeuft dabei nebenher: popen() gibt eine Leitung, die
// pollImport() Takt fuer Takt abfragt, so dass das Fenster weiterzeichnet,
// solange der Dialog offen ist. Der Export kann das nicht - doExport() liefert
// sein Ergebnis sofort, so steht es in transfer.h -, und haelt das Spiel
// deshalb an wie der modale Dialog unter Windows.
// ---------------------------------------------------------------------------

namespace
{
	std::string pickedPath;
	std::string pickedName;
	int   importStatus = STATUS_BUSY;
	bool  wantDialog = false;
	FILE* p_importPipe = 0;
	std::string importOutput;

	// Nur der Basisname.
	std::string getFilenameFromPath(const std::string& path)
	{
		const size_t cut = path.find_last_of('/');
		return cut == std::string::npos ? path : path.substr(cut + 1);
	}

	// Alles, was hier in eine Befehlszeile geht, ist ein Dateiname - und ein
	// Dateiname darf unter Linux fast jedes Zeichen enthalten, das Apostroph
	// eingeschlossen. In einfachen Anfuehrungszeichen ist ein Apostroph das
	// einzige, was die Zeichenkette beendet; ihn dafuer zu verlassen und
	// wieder zu betreten ist die uebliche Antwort.
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

	// Einmal suchen und merken: der Manager fragt sonst bei jedem Klick die
	// Shell zweimal.
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

	// Die Befehlszeile fuer einen der beiden Dialoge. suggestion leer heisst
	// "oeffnen", sonst "speichern unter".
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

	// Was das Dialogprogramm geschrieben hat, ohne den Zeilenumbruch am Ende.
	std::string trimmed(const std::string& text)
	{
		size_t end = text.length();
		while(end > 0 && (text[end - 1] == '\n' || text[end - 1] == '\r')) end--;
		return text.substr(0, end);
	}
}

bool beginImport()
{
	// Wie unter Windows nur vormerken: hier steckt der Aufruf mitten in der
	// Ereignisverteilung der GUI.
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
			// Ohne O_NONBLOCK bliebe das Spiel in read() stehen, bis der
			// Benutzer den Dialog schliesst - genau das soll es nicht.
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
			// Ende der Leitung: der Dialog ist zu. pclose() liefert den
			// Rueckgabewert, und der sagt, ob abgebrochen wurde.
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
	// Wie unter Windows wurde nichts zwischengelagert - die Datei des
	// Benutzers wurde gelesen, wo sie lag.
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

	// Der Name kommt aus dem eigenen Verzeichnis und traegt seine Endung
	// schon - er taugt unveraendert als Vorschlag.
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
	if(result != 0 || target.empty()) return false;   // abgebrochen, kein Fehler

	// kdialog haengt keine Endung an, zenity auch nicht. Ohne sie liesse sich
	// die Datei spaeter nicht wieder einlesen - classify() sieht zwar in die
	// Datei hinein, der Dialog beim Import filtert aber nach Endung.
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
