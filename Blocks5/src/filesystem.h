#ifndef _FILESYSTEM_H
#define _FILESYSTEM_H

/*** Klasse für das virtuelle Dateisystem ***/

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
	std::string getCurrentDir() const;
	std::string getPathDirectory(const std::string& path) const;
	std::string getPathFilename(const std::string& path) const;
	File* openFile(const std::string& filename, FileMode mode = FM_READ);
	bool fileExists(const std::string& filename);
	bool deleteFile(const std::string& filename);
	bool copyFile(const std::string& source, const std::string& dest);
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
	uint primes[256];
};

#endif