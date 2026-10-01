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

	// The folder the game sits in, with the shipped levels, campaigns and
	// skins; absolute and with a trailing slash. Absolute because main.cpp
	// mounts data.zip as the current directory, and a relative path would be
	// resolved inside the archive. Captured once at startup, since a file
	// dialog under Windows can change the working directory.
	const std::string& getGameDirectory() const { return gameDirectory; }

	// Where a content file really is: the game folder first, so that a stale
	// copy in the user directory cannot shadow the shipped one, then the user
	// directory - which is also the answer where neither has it. A player's
	// file (getPlayerFiles) is looked for the other way round.
	std::string resolveContentPath(const std::string& relative) const;

	// Does this file belong to the game - shipped and not a player's file?
	// The one answer to "undeletable", "un-overwritable" and "no editor may
	// save under this name".
	bool isShippedContent(const std::string& relative);

	// Files that ship with the game but belong to the player; the game
	// folder's copy is only a template. The list ends with a null pointer, and
	// copyOnFirstStart says whether main.cpp copies the file over once.
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

	// A rename where the platform can do one, which is atomic and what a save
	// swapping a side file into place wants; otherwise a copy and a delete.
	// It cannot across a mount (the browser stages an upload outside the home
	// directory's filesystem) nor for a member inside an archive.
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