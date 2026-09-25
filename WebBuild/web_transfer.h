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

	// The same for a PNG in memory, such as a screenshot: the IndexedDB is for
	// saved games, not a picture store.
	void downloadBytes(const void* p_data, unsigned int numBytes,
	                   const std::string& downloadName);

	// Opens the file dialog. JS picks one of the three staging paths by the
	// file's extension, so C still composes every path. Each must end in its
	// own extension (FileSystem::convertPath recognises an archive by ".zip/")
	// and lie outside the home directory, so a rejected file never reaches
	// the IndexedDB. False while a dialog is open or without user activation.
	bool openPicker(const std::string& stagingOgg,
	                const std::string& stagingXml,
	                const std::string& stagingZip,
	                unsigned int maxBytes);

	// Call once per logic tick. Returns IMPORT_IDLE while nothing has
	// finished; otherwise the status and the (unchecked) requested name.
	int pollImport(std::string& untrustedName);

	// Discards a dialog that is still open: a file picked after this is
	// neither written nor reported. The caller, which named the staging
	// files, deletes them.
	void abandon();

	// Writes the home directory through to IndexedDB now, not at the next
	// periodic flush.
	void syncHome();
}

#endif
#endif
