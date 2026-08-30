#include "pch.h"
#include "transfer.h"
#include "filesystem.h"
#include "file.h"
#include "campaign.h"
#include "util.h"

#ifdef __EMSCRIPTEN__
#include "web_transfer.h"
#elif defined(_WIN32)
#include <commdlg.h>
#endif

namespace
{
	std::string toLower(const std::string& text)
	{
		std::string result(text);
		for(size_t i = 0; i < result.length(); i++)
		{
			result[i] = static_cast<char>(tolower(static_cast<unsigned char>(result[i])));
		}
		return result;
	}

	// Nur der Basisname, egal mit welchem Trenner der Pfad gebaut war.
	std::string getFilenameFromPath(const std::string& path)
	{
		const size_t cut = path.find_last_of("/\\:");
		return cut == std::string::npos ? path : path.substr(cut + 1);
	}

	// Wo die vier Arten im Benutzerverzeichnis liegen.
	std::string directoryFor(Transfer::Kind kind)
	{
		const std::string home(FileSystem::inst().getAppHomeDirectory());
		switch(kind)
		{
		case Transfer::KIND_LEVEL:
		case Transfer::KIND_MUSIC:    return home + "levels/";
		case Transfer::KIND_CAMPAIGN: return home + "levels/campaigns/";
		case Transfer::KIND_SKIN:     return home + "levels/skins/";
		default:                      return "";
		}
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

	// Die Skins, die zum Spiel gehoeren. Ein Skin wird beim Import
	// ueberschrieben, weil sein Dateiname seine Kennung ist - also duerfen
	// diese vier nicht getroffen werden, sonst zerlegt ein Import die
	// mitgelieferte Kampagne. Die Liste ist die aus zip_skins.bat.
	bool isShippedSkin(const std::string& stem)
	{
		static const char* p_names[] = { "blocks_01", "blocks_02", "blocks_03", "space" };
		for(uint i = 0; i < sizeof(p_names) / sizeof(*p_names); i++)
		{
			if(stem == p_names[i]) return true;
		}
		return false;
	}

	// Ein Skin-Archiv, so geoeffnet, dass man hineinsieht: liegt eine
	// password.txt darin, steht dort das verschluesselte Passwort fuer alles
	// andere (siehe Level::getSkinFilename).
	std::string skinArchiveWithPassword(const std::string& archive)
	{
		FileSystem& fs = FileSystem::inst();
		const std::string pwFile(archive + "/password.txt");
		if(!fs.fileExists(pwFile)) return archive;
		return archive + "[" + fs.readStringFromFile(pwFile) + "]";
	}

	// Einen freien Namen in dir finden: stem.ext, sonst stem_2.ext ...
	std::string uniqueName(const std::string& dir,
						   const std::string& stem,
						   const std::string& ext)
	{
		FileSystem& fs = FileSystem::inst();
		std::string name(stem + ext);
		for(int n = 2; n <= 99 && fs.fileExists(dir + name); n++)
		{
			char temp[128] = "";
			sprintf(temp, "%s_%d%s", stem.c_str(), n, ext.c_str());   // stem <= 64 Zeichen
			name = temp;
		}
		return name;
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
					std::string& errorId)
{
	FileSystem& fs = FileSystem::inst();
	errorId = "";

	if(kind == KIND_CAMPAIGN)
	{
		// Eine Kampagne muss sich auch laden lassen, nicht nur eine
		// campaign.xml enthalten.
		if(!Campaign::isImportableArchive(path))
		{
			errorId = "$TR_ERROR_BROKEN";
			return "";
		}
		const std::string name(Campaign::installArchive(path, untrustedName));
		if(name.empty()) errorId = "$TR_ERROR_FAILED";
		return name;
	}

	const std::string dir(directoryFor(kind));
	const std::string stem(sanitizeFilenameStem(untrustedName, "import"));

	if(kind == KIND_SKIN)
	{
		// Der Dateiname eines Skins ist seine Kennung: ein Level sagt
		// skin0="space" und der Lader sucht levels/skins/space.zip. Wuerde
		// ein schon vergebener Name hier zu space_2.zip ausweichen, blieben
		// alle Level, die "space" nennen, genauso kaputt wie vorher - nur
		// ohne sichtbaren Grund. Ein Skin ueberschreibt deshalb.
		if(isShippedSkin(stem))
		{
			errorId = "$TR_ERROR_SKIN_RESERVED";
			return "";
		}
		const std::string name(stem + ".zip");
		if(!fs.copyFile(path, dir + name))
		{
			errorId = "$TR_ERROR_FAILED";
			return "";
		}
		return name;
	}

	const std::string name(uniqueName(dir, stem, extensionFor(kind)));
	if(!fs.copyFile(path, dir + name))
	{
		errorId = "$TR_ERROR_FAILED";
		return "";
	}
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

std::string suggestedFilename(Kind kind, const std::string& name)
{
	// Der Name kommt aus dem eigenen Verzeichnis, taugt also schon als
	// Dateiname; die Endung steht ohnehin schon daran.
	(void)kind;
	return name;
}

bool exportTo(Kind kind, const std::string& name, const std::string& destPath)
{
	FileSystem& fs = FileSystem::inst();
	const std::string source(directoryFor(kind) + name);
	if(!fs.fileExists(source)) return false;

	// Drei der vier mitgelieferten Skins sind mit einem Passwort gepackt
	// (zip_skins.bat), das als password.txt verschluesselt im Archiv liegt.
	// Byte fuer Byte kopiert bekaeme der Empfaenger ein Archiv, das er nicht
	// aufbekommt - und "den Skin forken" ist genau der Grund, aus dem man
	// einen exportiert. Also entschluesselt neu packen.
	if(kind == KIND_SKIN)
	{
		const std::string opened(skinArchiveWithPassword(source));
		if(opened != source)
		{
			bool ok = true;
			std::list<std::string> members = fs.listDirectory(source + "/");
			for(std::list<std::string>::const_iterator i = members.begin(); i != members.end(); ++i)
			{
				// password.txt gehoert zum verschluesselten Archiv und hat im
				// offenen nichts zu suchen.
				if(*i == "password.txt") continue;
				if(!isSafeMemberName(*i)) continue;
				ok &= fs.copyFile(opened + "/" + *i, destPath + "/" + *i);
			}
			return ok;
		}
	}

	return fs.copyFile(source, destPath);
}

// ---------------------------------------------------------------------------
// Der Dateidialog. Zwei Welten, eine Schnittstelle.
// ---------------------------------------------------------------------------

#ifdef __EMSCRIPTEN__

namespace
{
	// C gibt alle drei moeglichen Ziele vor und JS sucht sich nach der Endung
	// eines davon aus. Damit setzt weiterhin niemand ausser C einen Pfad
	// zusammen. Die Zwischendatei liegt ausserhalb des Benutzerverzeichnisses,
	// damit eine abgelehnte Datei gar nicht erst in die IndexedDB kommt.
	const char* const p_stagingOgg = "/blocks5_import.ogg";
	const char* const p_stagingXml = "/blocks5_import.xml";
	const char* const p_stagingZip = "/blocks5_import.zip";

	std::string stagingFor(const std::string& untrustedName)
	{
		const std::string ext(toLower(getFilenameExtension(untrustedName)));
		if(ext == "ogg") return p_stagingOgg;
		if(ext == "xml") return p_stagingXml;
		if(ext == "zip") return p_stagingZip;
		return "";
	}

	std::string g_staging;
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

	g_staging = stagingFor(untrustedName);
	if(g_staging.empty()) return STATUS_UNKNOWN;
	path = g_staging;
	return STATUS_OK;
}

void finishImport()
{
	if(g_staging.empty()) return;
	FileSystem::inst().deleteFile(g_staging);
	g_staging = "";
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
	g_staging = "";
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
	WebTransfer::download(tmp, suggestedFilename(kind, name));
	FileSystem::inst().deleteFile(tmp);
	return true;
}

#elif defined(_WIN32)

namespace
{
	std::string g_pickedPath;
	std::string g_pickedName;
	int g_status = STATUS_BUSY;

	// Ein modaler Dialog laesst die Hauptschleife stehen. Danach glaubt SDL
	// unter Umstaenden noch, eine Maustaste sei unten - der naechste
	// SDL_PollEvent raeumt das auf, aber der Klick, der den Dialog geoeffnet
	// hat, darf nicht noch einmal durchschlagen.
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
}

bool beginImport()
{
	char filter[128] = "";
	buildFilter(filter, sizeof(filter));

	char file[MAX_PATH] = "";
	OPENFILENAMEA ofn;
	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = GetActiveWindow();
	ofn.lpstrFilter = filter;
	ofn.lpstrFile = file;
	ofn.nMaxFile = sizeof(file);
	// OFN_NOCHANGEDIR ist Pflicht: das Spiel oeffnet data.zip relativ zum
	// Arbeitsverzeichnis, und der Dialog wuerde es sonst verstellen.
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_HIDEREADONLY;

	if(!GetOpenFileNameA(&ofn))
	{
		g_status = STATUS_CANCELLED;
		return true;
	}

	g_pickedPath = file;
	g_pickedName = getFilenameFromPath(g_pickedPath);
	g_status = STATUS_OK;
	return true;
}

int pollImport(std::string& path, std::string& untrustedName)
{
	const int status = g_status;
	if(status == STATUS_BUSY) return STATUS_BUSY;
	g_status = STATUS_BUSY;
	path = g_pickedPath;
	untrustedName = g_pickedName;
	return status;
}

void finishImport()
{
	// Unter Windows wurde nichts zwischengelagert - die Datei des Benutzers
	// wurde gelesen, wo sie lag.
	g_pickedPath = "";
	g_pickedName = "";
}

void abandonImport()
{
	g_status = STATUS_BUSY;
	finishImport();
}

bool doExport(Kind kind, const std::string& name, std::string& errorId)
{
	errorId = "";

	char filter[128] = "";
	buildFilter(filter, sizeof(filter));

	char file[MAX_PATH] = "";
	const std::string suggested(suggestedFilename(kind, name));
	strncpy(file, suggested.c_str(), sizeof(file) - 1);

	OPENFILENAMEA ofn;
	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = GetActiveWindow();
	ofn.lpstrFilter = filter;
	ofn.lpstrFile = file;
	ofn.nMaxFile = sizeof(file);
	ofn.lpstrDefExt = extensionFor(kind) + 1;   // ohne den Punkt
	ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_HIDEREADONLY;

	if(!GetSaveFileNameA(&ofn)) return false;   // abgebrochen, kein Fehler

	// Ein Skin wird neu gepackt, und dazu muss das Ziel leer sein.
	FileSystem::inst().deleteFile(file);
	if(!exportTo(kind, name, file))
	{
		errorId = "$TR_ERROR_FAILED";
		return false;
	}
	return true;
}

#else
#error NOT IMPLEMENTED
#endif

}
