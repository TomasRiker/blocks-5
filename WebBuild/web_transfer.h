#ifndef _WEB_TRANSFER_H
#define _WEB_TRANSFER_H
#ifdef __EMSCRIPTEN__

/*** File exchange between the virtual filesystem and the browser ***/

// The bridge layer under Transfer (src/transfer.h): it knows only blobs, file
// dialogs and IndexedDB, nothing of levels, campaigns, music and skins.

#include <string>

namespace WebTransfer
{
	enum ImportStatus
	{
		IMPORT_IDLE       = 0,   // nothing has happened
		IMPORT_OK         = 1,   // file is in one of the three staging files
		IMPORT_CANCELLED  = 2,   // the user cancelled
		IMPORT_TOO_BIG    = 3,
		IMPORT_WRONG_TYPE = 4,   // extension is none of the three
		IMPORT_READ_ERROR = 5
	};

	// Hands the content of vfsPath to the browser as a download.
	void download(const std::string& vfsPath, const std::string& downloadName);

	// The same for something that is not a file at all - a screenshot, say,
	// which has nowhere to go in the browser: the IndexedDB is there for saved
	// games and not as a picture store.
	void downloadBytes(const void* p_data, unsigned int numBytes,
	                   const std::string& downloadName);

	// Opens the file dialog. The caller hands over all three possible targets
	// and JS picks one of them by extension, so C still composes every path.
	// All three must end in their own extension (FileSystem::convertPath
	// recognises an archive by ".zip/") and must NOT lie under the home
	// directory, so a rejected file never reaches the IndexedDB in the first
	// place.
	// Returns false if a dialog is already running or the browser sees no
	// user activation right now.
	bool openPicker(const std::string& stagingOgg,
	                const std::string& stagingXml,
	                const std::string& stagingZip,
	                unsigned int maxBytes);

	// Call once per logic tick. Returns IMPORT_IDLE while nothing has
	// finished; otherwise the status and the (unchecked) requested name.
	int pollImport(std::string& untrustedName);

	// Discards a dialog that is still open. The caller clears the staging
	// files away itself - it named them, after all.
	void abandon();

	// Forces an FS.syncfs to land an import in IndexedDB at once.
	void syncHome();
}

#endif
#endif
