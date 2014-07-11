#include "pch.h"
#include "file_real.h"
#include "filesystem.h"

File_Real::File_Real(const std::string& filename,
					 int mode) : File(mode)
{
	p_handle = 0;

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

		fseek(p_handle, 0, SEEK_END);
		size = ftell(p_handle);
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
		// Dateien auflisten
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
#error NOT IMPLEMENTED
#endif
	}
	else if(mode == FileSystem::FM_DELETE)
	{
		remove(filename.c_str());
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
	uint pointer = ftell(p_handle);
	return pointer;
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