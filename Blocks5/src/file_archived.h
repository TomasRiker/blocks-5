#ifndef _FILE_ARCHIVED_H
#define _FILE_ARCHIVED_H

/*** Klasse für eine archivierte Datei ***/

#include "file.h"
#include <zip.h>

class File_Archived : public File
{
	friend class FileSystem;

public:
	uint read(void* p_dest, uint numBytes);
	uint write(const void* p_src, uint numBytes);
	std::list<std::string> listDirectory();
	uint tell() const;
	bool isEOF() const;
	uint getSize() const;
	bool seek(uint newReadPointer);
	bool finish();

private:
	File_Archived(const std::string& archiveFilename, const std::string& objectName, const std::string& password, int mode);
	~File_Archived();

	static int deleteArchivedFile(const std::string& archiveFilename, const std::string& objectName);

	char* p_data;
	uint pointer;
	uint size;
	bool eof;
	std::list<std::string> directory;

	std::string archiveFilename;
	std::string objectName;
	std::string password;
	zipFile outArchive;
	uint bufferSize;
};

#endif