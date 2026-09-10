#ifndef _PROGRESSDB_H
#define _PROGRESSDB_H

#include "singleton.h"

/*** Class for saving and retrieving progress ***/

// The class holds nothing: every query reads the file and every mark writes
// it. That is what makes the Manager's import, merge and delete work at all -
// a copy in memory would answer from what was there when the game started and
// the next completed level would write it straight back over the import.
//
// The file is read once per visit to the level selection, not per frame: the
// screen keeps the answer for as long as it is on the screen. GS_SelectLevel
// does that in onGetFocus(), which is where a pop back from a played level
// arrives - onEnter() runs only for a state that is pushed.
class ProgressDB : public Singleton<ProgressDB>
{
	friend class Singleton<ProgressDB>;

public:
	// Campaign (by the key below) to the levels of it that are solved.
	typedef std::unordered_map<std::string, std::set<uint> > Progress;

	// The whole database. An empty filename means the player's own; any other
	// reads a foreign file, which is how a merge gets at what it is merging.
	//
	// p_intact answers whether what comes back is the whole of what lies on
	// the disk. It is false in one case only: a save was interrupted and the
	// backup could not be put back. Nothing may then write, because the one
	// copy of the player's progress is still standing under the other name -
	// an empty answer would otherwise be taken for "nothing solved yet" and
	// written back over it.
	Progress query(const std::string& filename = std::string(), bool* p_intact = 0);

	// Mark levels as solved and write the database out. It reads the file
	// first, so this adds to what is on the disk now rather than to what was
	// there when the screen was opened - and a merge is nothing more than
	// this call with another database's contents.
	bool markSolved(const std::vector<std::pair<std::string, uint> >& solved);

	// Delete the database, its backup with it: a backup left behind would be
	// restored by the next query and undo the deletion.
	bool remove();

	// Put a file the player brought in in place of the database, through the
	// same backup as a save: the copy truncates what is there, and a failure
	// halfway would otherwise leave neither the old one nor the new one.
	bool installFrom(const std::string& source);

	// Is there a database at all? Under either name: right after an
	// interrupted save the whole of it is standing under the backup's.
	bool exists();

	// The player's own database, and the name the old one is put aside under
	// while a new one is written.
	static std::string getFilename();
	static std::string getBackupFilename();

	// Does this archive hold a progress database? Asked of a file somebody
	// wants to import, and answered from the archive's table of contents,
	// which lies open even in an encrypted zip.
	static bool isProgressArchive(const std::string& filename);

	// Can it be read? The stricter question, asked before an import replaces
	// a good database with a damaged one.
	static bool canRead(const std::string& filename);

	// The key to a campaign: its bare filename. See progressdb.cpp - the full
	// path will not do, because a campaign file can change folder.
	static std::string keyFor(const std::string& campaign);

private:
	ProgressDB();
	~ProgressDB();

	// Reads one file. False when it is not there or does not parse, which is
	// the same answer to the one caller that cares: put the backup back.
	static bool read(const std::string& filename, Progress& progress);
	static bool write(const Progress& progress);
};

#endif
