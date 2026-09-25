#ifndef _TRANSFER_H
#define _TRANSFER_H

/*** Moving files between the game and the outside world ***/

// Everything here exists on all three platforms; only the file dialog
// differs. Under Windows it is modal and under Linux a zenity or kdialog
// process, and both yield a path the game reads where it lies; in the
// browser it is asynchronous and puts a copy into a staging file.
// beginImport()/pollImport() hide that.

namespace Transfer
{
	enum Kind
	{
		KIND_NONE = 0,
		KIND_LEVEL,
		KIND_CAMPAIGN,
		KIND_MUSIC,
		KIND_SKIN,

		// The player's progress: unlike the other four, one file with one
		// name in the user directory itself, and nothing of it ever ships.
		KIND_PROGRESS
	};

	// Works out the kind by content: music by a Vorbis stream's first packet,
	// a level by its XML root, and a .zip, in any case, by what it holds -
	// campaign.xml, tileset.xml with sprites.png, or progress.xml.
	Kind classify(const std::string& path);

	// The name install() would give this file, and whether it would replace
	// something. install() works with the same two, so the player can be
	// asked before anything is written.
	std::string targetName(Kind kind, const std::string& untrustedName);
	bool wouldReplace(Kind kind, const std::string& untrustedName);

	// Takes a file into the user directory and returns the filename assigned
	// to it, or "" and then sets errorId. untrustedName is only a suggestion;
	// the destination path is composed here. A file of the same name is
	// replaced, which p_replaced reports; the names from isBuiltIn() are not.
	std::string install(Kind kind,
						const std::string& path,
						const std::string& untrustedName,
						std::string& errorId,
						bool* p_replaced = 0);

	// Everything of this kind from both roots, sorted: the game folder holds
	// what ships, the user directory what is the player's own.
	std::vector<std::string> list(Kind kind);

	// Does this file belong to the game? name is the filename with extension,
	// the way list() delivers it. Shipped means it lies in the game folder -
	// and then an import must not take the name, the Manager must not delete
	// it and no editor may save under it.
	bool isBuiltIn(Kind kind, const std::string& name);

	// Can this file be deleted? Only the player's own version in the user
	// directory: never anything shipped, and of the example levels only once
	// the player has saved one of them themselves.
	bool isRemovable(Kind kind, const std::string& name);

	// Deletes what list() delivered, provided isRemovable() allows it.
	bool remove(Kind kind, const std::string& name, std::string& errorId);

	enum Status
	{
		STATUS_BUSY = 0,
		STATUS_OK,
		STATUS_CANCELLED,
		STATUS_TOO_BIG,
		STATUS_UNKNOWN,
		STATUS_FAILED,

		// Linux with neither zenity nor kdialog installed.
		STATUS_NO_DIALOG
	};

	// false means "not possible right now, please click again": the browser
	// only hands over a file dialog while it still considers a click of the
	// user's to be fresh.
	bool beginImport();

	// Call every logic tick. On STATUS_OK path holds a readable file and
	// untrustedName the name wished for from outside.
	int pollImport(std::string& path, std::string& untrustedName);

	// Call after STATUS_OK once the file has been processed.
	void finishImport();

	// On leaving the menu: discards a dialog that is still open.
	void abandonImport();

	// Returns its answer at once: a modal dialog on the desktop, a download in
	// the browser. false means cancelled or failed; on a plain cancel errorId
	// stays empty.
	bool doExport(Kind kind, const std::string& name, std::string& errorId);
}

#endif
