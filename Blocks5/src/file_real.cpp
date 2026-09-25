#include "pch.h"
#ifndef _WIN32
#include <dirent.h>
#include <sys/stat.h>
#endif
#include "file_real.h"
#include "filesystem.h"

File_Real::File_Real(const std::string& filename,
					 int mode) : File(mode)
{
	p_handle = 0;
	size = 0;

	if(mode == FileSystem::FM_READ)
	{
		p_handle = fopen(filename.c_str(), "rb");
		if(!p_handle)
		{
			printfLog("+ ERROR: Could not open file \"%s\" for reading.\n",
					  filename.c_str());
			error = 1;
			return;
		}

		// A size the 32-bit interface cannot carry is refused rather than
		// read as some other number, as ftell's -1 past 2 GB where long is 32
		// bits would be: FileSystem::readStringFromFile allocates size + 1.
		fseek(p_handle, 0, SEEK_END);
		const long end = ftell(p_handle);
		if(end < 0 || static_cast<unsigned long>(end) >= 0xFFFFFFFFul)
		{
			printfLog("+ ERROR: File \"%s\" is too large to read.\n",
					  filename.c_str());
			error = 1;
			return;
		}
		size = static_cast<uint>(end);
		rewind(p_handle);
	}
	else if(mode == FileSystem::FM_WRITE)
	{
		p_handle = fopen(filename.c_str(), "wb");
		if(!p_handle)
		{
			printfLog("+ ERROR: Could not open file \"%s\" for writing.\n",
					  filename.c_str());
			error = 1;
			return;
		}

		size = 0;
	}
	else if(mode == FileSystem::FM_TEST)
	{
		p_handle = fopen(filename.c_str(), "r");
		error = p_handle ? 0 : 1;
	}
	else if(mode == FileSystem::FM_LIST)
	{
		// list the files
#ifdef _WIN32
		WIN32_FIND_DATAA findData;
		HANDLE find = FindFirstFileA((filename + "/*.*").c_str(), &findData);
		if(find != INVALID_HANDLE_VALUE)
		{
			do
			{
				std::string filename = findData.cFileName;
				if(filename != "." && filename != ".." && !(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) directory.push_back(filename);

			} while(FindNextFileA(find, &findData));

			FindClose(find);
		}
#else
		if(DIR* p_dir = ::opendir(filename.c_str()))
		{
			while(struct dirent* p_ent = ::readdir(p_dir))
			{
				const std::string entry(p_ent->d_name);
				if(entry == "." || entry == "..") continue;
				struct stat st;
				if(::stat((filename + "/" + entry).c_str(), &st) == 0 && S_ISDIR(st.st_mode)) continue;
				directory.push_back(entry);
			}
			::closedir(p_dir);
		}
#endif
	}
	else if(mode == FileSystem::FM_DELETE)
	{
		// remove() returns 0 only once the file is gone. A read-only or open
		// file stays, and FileSystem::deleteFile() must not report it deleted.
		if(remove(filename.c_str()) != 0) error = 9;
	}
	else
	{
		printfLog("+ ERROR: Invalid file mode.\n");
		error = 1;
		return;
	}

	eof = false;
}

File_Real::~File_Real()
{
	if(p_handle) fclose(p_handle);
}

bool File_Real::finish()
{
	if(mode != FileSystem::FM_WRITE || !p_handle) return false;

	// A write lands in stdio's buffer, and a full disk shows only when the
	// buffer goes out: here, and not in any result write() gave.
	const bool closed = fclose(p_handle) == 0;
	p_handle = 0;
	return closed;
}

bool File_Real::isEOF() const
{
	return eof;
}

uint File_Real::getSize() const
{
	return mode == FileSystem::FM_READ ? size : tell();
}

uint File_Real::tell() const
{
	// A listing or a deletion holds no handle and has no position.
	if(!p_handle) return 0;
	return static_cast<uint>(ftell(p_handle));
}

uint File_Real::read(void* p_dest,
					 uint numBytes)
{
	if(mode != FileSystem::FM_READ)
	{
		printfLog("+ ERROR: Can't read from a file opened in write mode.\n");
		return 0;
	}

	uint numBytesRead = static_cast<uint>(fread(p_dest, 1, numBytes, p_handle));
	eof = feof(p_handle) ? true : false;
	return numBytesRead;
}

uint File_Real::write(const void* p_src,
					  uint numBytes)
{
	if(mode != FileSystem::FM_WRITE)
	{
		printfLog("+ ERROR: Can't write to a file opened in read mode.\n");
		return 0;
	}

	uint numBytesWritten = static_cast<uint>(fwrite(p_src, 1, numBytes, p_handle));
	return numBytesWritten;
}

std::list<std::string> File_Real::listDirectory()
{
	return directory;
}

bool File_Real::seek(uint newReadPointer)
{
	if(mode != FileSystem::FM_READ)
	{
		printfLog("+ ERROR: Can't seek in a file opened in write mode.\n");
		return false;
	}

	if(newReadPointer > size) return false;
	fseek(p_handle, newReadPointer, SEEK_SET);
	eof = feof(p_handle) ? true : false;
	return true;
}