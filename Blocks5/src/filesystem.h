#ifndef _FILESYSTEM_H
#define _FILESYSTEM_H

/*** Class for the virtual filesystem ***/

#include "file.h"

class FileSystem : public Singleton<FileSystem>
{
	friend class Singleton<FileSystem>;

public:
	enum FileMode
	{
		FM_READ,
		FM_WRITE,
		FM_TEST,
		FM_LIST,
		FM_DELETE
	};

	void closeFile(File* p_file);
	std::string evalPath(const std::string& path) const;
	std::string getAppHomeDirectory() const;

	// The folder the game sits in, absolute and with a trailing slash. It
	// holds the shipped levels, campaigns and skins. Absolute because main.cpp
	// mounts data.zip as the root: a relative path would be resolved inside
	// the archive at runtime instead of on disk. Remembered once at startup,
	// because a file dialog under Windows can change the working directory.
	const std::string& getGameDirectory() const { return gameDirectory; }

	// Where a content file really is: the game folder first, then the user
	// directory. The game folder wins, or a stale copy in the user directory
	// could shadow the shipped one. If neither exists, the path in the user
	// directory comes back: that is where it would have belonged.
	std::string resolveContentPath(const std::string& relative) const;

	// Does this file belong to the game? That is simultaneously the answer to
	// "undeletable", "un-overwritable" and "no editor may save under this
	// name" - no list of its own is needed for it.
	bool isShippedContent(const std::string& relative);

	// Files that belong to the player even though they ship with the game: the
	// version in the game folder is only a template for them. The list is
	// terminated by a null pointer; copyOnFirstStart says whether main.cpp
	// copies one over once.
	struct PlayerFile
	{
		const char* p_path;
		bool copyOnFirstStart;
	};
	static const PlayerFile* getPlayerFiles();
	bool belongsToPlayer(const std::string& relative) const;
	std::string getCurrentDir() const;
	std::string getPathDirectory(const std::string& path) const;
	std::string getPathFilename(const std::string& path) const;
	File* openFile(const std::string& filename, FileMode mode = FM_READ);
	bool fileExists(const std::string& filename);
	bool deleteFile(const std::string& filename);
	bool copyFile(const std::string& source, const std::string& dest);

	// Put the file at the new path: a rename where the platform can do one,
	// and a copy followed by a delete where it cannot. It cannot across a
	// mount - the browser stages an upload outside the home directory, which
	// is a filesystem of its own - and it cannot for a member inside an
	// archive. Where it is a rename it is also atomic, which is what a save
	// that swaps a side file into place is after.
	bool renameFile(const std::string& source, const std::string& dest);
	bool createDirectory(const std::string& directory);
	bool deleteDirectory(const std::string& directory);
	std::string readStringFromFile(const std::string& filename);
	bool writeStringToFile(const std::string& text, const std::string& filename);
	void popCurrentDir();
	void pushCurrentDir(const std::string& dir);
	void convertPath(const std::string& path, std::string& filePath, std::string& objectName, std::string& password) const;
	std::string evalRelativePath(const std::string& path, const std::string& basePath) const;
	bool isAbsolutePath(const std::string& path) const;
	std::list<std::string> listDirectory(const std::string& directory);

private:
	FileSystem();
	~FileSystem();

	std::stack<std::string> dirStack;
	std::string gameDirectory;
	uint primes[256];
};

#endif