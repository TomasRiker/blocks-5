#ifndef _FILE_REAL_H
#define _FILE_REAL_H

/*** Klasse für eine echte Datei ***/

#include "file.h"

class File_Real : public File
{
	friend class FileSystem;

public:
	uint read(void* p_dest, uint numBytes);
	uint write(const void* p_src, uint numBytes);
	std::list<std::string> listDirectory();
	bool seek(uint newReadPointer);
	uint tell() const;
	bool isEOF() const;
	uint getSize() const;

private:
	File_Real(const std::string& filename, int mode);
	~File_Real();

	FILE* p_handle;
	uint size;
	bool eof;
	std::list<std::string> directory;
};

#endif