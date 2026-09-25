#ifndef _PROGRESSDB_H
#define _PROGRESSDB_H

#include "singleton.h"

/*** Class for saving and retrieving progress ***/

// Holds nothing: every query reads the file, every mark reads and writes it.
// A copy in memory would answer from what was there at startup, and the next
// completed level would write it back over an import, a merge or a delete.
//
// GS_SelectLevel keeps the answer while it is shown and reads it again in
// onGetFocus(), where a pop back from a played level arrives - a pop does not
// run onEnter().
class ProgressDB : public Singleton<ProgressDB>
{
	friend class Singleton<ProgressDB>;

public:
	// Campaign (by the key below) to the levels of it that are solved.
	typedef std::unordered_map<std::string, std::set<uint> > Progress;

	// The whole database: the player's own for an empty filename, otherwise a
	// foreign file, which is how a merge reads what it merges.
	//
	// *p_intact is false only when an interrupted save's backup could not be
	// put back. Nothing may write then: the player's progress still stands
	// under the backup's name, and the empty answer would be written over it
	// as "nothing solved yet".
	Progress query(const std::string& filename = std::string(), bool* p_intact = 0);

	// Marks levels solved and writes the database. It reads the file first, so
	// it adds to what is on the disk now rather than to what was there when
	// the screen opened; a merge is this call with another database's contents.
	bool markSolved(const std::vector<std::pair<std::string, uint> >& solved);

	// Delete the database, its backup with it: a backup left behind would be
	// restored by the next query and undo the deletion.
	bool remove();

	// Puts an imported file in place of the database, through the same backup
	// as a save: the copy truncates what is there, and a failure halfway would
	// otherwise leave neither the old database nor the new one.
	bool installFrom(const std::string& source);

	// Is there a database, under either name? Right after an interrupted save
	// the whole of it stands under the backup's.
	bool exists();

	// The player's own database, and the name the old one is put aside under
	// while a new one is written.
	static std::string getFilename();
	static std::string getBackupFilename();

	// Does this archive hold a progress database? Answered from the table of
	// contents, which lies open even in an encrypted zip.
	static bool isProgressArchive(const std::string& filename);

	// Can it be read? The stricter question, asked before an import replaces
	// a good database with a damaged one.
	static bool canRead(const std::string& filename);

	// The key to a campaign: its bare filename, since a campaign file can
	// change folder (see progressdb.cpp).
	static std::string keyFor(const std::string& campaign);

private:
	ProgressDB();
	~ProgressDB();

	// False when the file is missing or does not parse; query() answers both
	// alike, by putting back a backup if there is one.
	static bool read(const std::string& filename, Progress& progress);
	static bool write(const Progress& progress);
};

#endif
