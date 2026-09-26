#include "pch.h"
#include "file_archived.h"
#include "filesystem.h"
#include <zip.h>
#include <unzip.h>
#include <new>

File_Archived::File_Archived(const std::string& archiveFilename,
							 const std::string& objectName,
							 const std::string& password,
							 int mode): File(mode)
{
	p_data = 0;
	pointer = 0;
	size = 0;
	eof = false;
	// Before any early return: the destructor asks finish() to close it.
	outArchive = 0;

	if(archiveFilename.empty() || objectName.empty())
	{
		error = 1;
		return;
	}

	bool readMode = mode == FileSystem::FM_READ;
	bool testMode = mode == FileSystem::FM_TEST;
	bool listMode = mode == FileSystem::FM_LIST;
	if(readMode || testMode || listMode)
	{
		// open the archive
		unzFile archive = unzOpen(archiveFilename.c_str());
		if(!archive)
		{
			if(!testMode)
			{
				printfLog("+ ERROR: Could not open archive file \"%s\" for reading.\n",
						  archiveFilename.c_str());
			}

			error = 2;
			return;
		}

		if(!listMode)
		{
			// Found without case on every platform, where minizip's default
			// ignores it under Windows alone: an archive must hold the same
			// members wherever it is played. deleteArchivedFile() matches
			// without case too, and Campaign::save() stores one of two track
			// names that differ only there.
			int r = unzLocateFile(archive, objectName.c_str(), 2);
			if(r != UNZ_OK)
			{
				if(!testMode)
				{
					printfLog("+ ERROR: Could not locate file \"%s\" in archive \"%s\" (Error: %d).\n",
							  objectName.c_str(),
							  archiveFilename.c_str(),
							  r);
				}

				unzClose(archive);
				error = 3;
				return;
			}
		}
		else
		{
			// List the files. minizip copies at most the buffer's worth of a
			// name but reports its full length, a 16-bit field in the archive;
			// a name that does not fit is skipped rather than read past the
			// end of the buffer.
			int r = unzGoToFirstFile(archive);
			while(r == UNZ_OK)
			{
				unz_file_info info;
				char temp[256] = "";
				if(unzGetCurrentFileInfo(archive, &info, temp, sizeof(temp), 0, 0, 0, 0) != UNZ_OK) break;
				if(info.size_filename < sizeof(temp))
				{
					std::string filename(temp, info.size_filename);
					if(filename.find_first_of('/') == std::string::npos) directory.push_back(filename);
				}
				r = unzGoToNextFile(archive);
			}
		}

		if(testMode || listMode)
		{
			unzClose(archive);
			return;
		}

		// query the file information
		unz_file_info info;
		if(unzGetCurrentFileInfo(archive, &info, 0, 0, 0, 0, 0, 0) != UNZ_OK)
		{
			printfLog("+ ERROR: Could not read the entry of \"%s\" in archive \"%s\".\n",
					  objectName.c_str(),
					  archiveFilename.c_str());
			unzClose(archive);
			error = 4;
			return;
		}

		// The size is the archive's claim, and an archive can come from
		// anybody: the Manager imports campaigns and skins. The largest real
		// members are music tracks of about two megabytes; 64 MB is nearly an
		// hour of Vorbis at 160 kbit/s. Above that, or without the memory, the
		// member is refused, since nothing here would catch a throwing
		// allocation.
		const uLong MAX_MEMBER_SIZE = 64 * 1024 * 1024;
		if(info.uncompressed_size > MAX_MEMBER_SIZE)
		{
			printfLog("+ ERROR: \"%s\" in archive \"%s\" claims %lu bytes; a member may hold 64 MB at most.\n",
					  objectName.c_str(),
					  archiveFilename.c_str(),
					  static_cast<unsigned long>(info.uncompressed_size));
			unzClose(archive);
			error = 11;
			return;
		}

		// allocate the memory
		size = static_cast<uint>(info.uncompressed_size);
		p_data = new(std::nothrow) char[size ? size : 1];
		if(!p_data)
		{
			printfLog("+ ERROR: Out of memory reading \"%s\" from archive \"%s\".\n",
					  objectName.c_str(),
					  archiveFilename.c_str());
			size = 0;
			unzClose(archive);
			error = 11;
			return;
		}

		// read the object in
		int r;
		if(!password.empty()) r = unzOpenCurrentFilePassword(archive, password.c_str());
		else r = unzOpenCurrentFile(archive);
		if(r != UNZ_OK)
		{
			printfLog("+ ERROR: Could not open file \"%s\" in archive \"%s\" for reading (Error: %d).\n",
					  objectName.c_str(),
					  archiveFilename.c_str(),
					  r);
			delete[] p_data;
			p_data = 0;
			unzClose(archive);
			error = 5;
			return;
		}

		if(unzReadCurrentFile(archive, p_data, static_cast<unsigned int>(size)) != size)
		{
			printfLog("+ ERROR: Could not read from file \"%s\" in archive \"%s\".\n",
					  objectName.c_str(),
					  archiveFilename.c_str());
			delete[] p_data;
			p_data = 0;
			unzCloseCurrentFile(archive);
			unzClose(archive);
			error = 6;
			return;
		}

		unzCloseCurrentFile(archive);
		unzClose(archive);
	}
	else if(mode == FileSystem::FM_WRITE)
	{
		this->archiveFilename = archiveFilename;
		this->objectName = objectName;
		this->password = password;

		// Does the archive exist already?
		FILE* p_file = fopen(archiveFilename.c_str(), "rb");
		bool archiveExists = p_file ? true : false;
		if(p_file) fclose(p_file);

		bool objectExists = false;
		if(archiveExists)
		{
			// Does the archived file exist already? Without case, as when
			// reading.
			unzFile temp = unzOpen(archiveFilename.c_str());
			if(temp)
			{
				int r = unzLocateFile(temp, objectName.c_str(), 2);
				if(r == UNZ_OK) objectExists = true;
				unzClose(temp);
			}

			if(objectExists)
			{
				// delete the object
				int r = deleteArchivedFile(archiveFilename, objectName);
				if(r == -1) archiveExists = false;
				else if(r != 1)
				{
					// The old member is still in there, and one appended
					// under the same name would lose to it.
					error = 10;
					return;
				}
			}
		}

		// open the archive for appending files
		outArchive = zipOpen(archiveFilename.c_str(), archiveExists ? APPEND_STATUS_ADDINZIP : APPEND_STATUS_CREATE);
		if(!outArchive)
		{
			printfLog("+ ERROR: Could not open archive file \"%s\" for writing.\n",
					  archiveFilename.c_str());
			error = 7;
			return;
		}

		bufferSize = 0;
	}
	else if(mode == FileSystem::FM_DELETE)
	{
		// Delete the object. 1 means deleted, -1 deleted and the archive left
		// empty in the process, 0 not found and -2 an archive that could not
		// be rewritten.
		const int r = deleteArchivedFile(archiveFilename, objectName);
		if(r == 0 || r == -2) error = 9;
	}
	else
	{
		printfLog("+ ERROR: Invalid file mode.\n");
		error = 8;
		return;
	}
}

File_Archived::~File_Archived()
{
	if(mode == FileSystem::FM_WRITE) finish();

	delete[] p_data;
}

bool File_Archived::isEOF() const
{
	return eof;
}

uint File_Archived::getSize() const
{
	return mode == FileSystem::FM_READ ? size : tell();
}

uint File_Archived::tell() const
{
	return pointer;
}

uint File_Archived::read(void* p_dest,
						 uint numBytes)
{
	if(mode != FileSystem::FM_READ)
	{
		printfLog("+ ERROR: Can't read from a file opened in write mode.\n");
		return 0;
	}

	// Does that fit?
	if(pointer + numBytes > size)
	{
		numBytes = size - pointer;
		eof = true;
	}

	memcpy(p_dest, p_data + pointer, numBytes);
	pointer += numBytes;

	return numBytes;
}

uint File_Archived::write(const void* p_src,
						  uint numBytes)
{
	if(mode != FileSystem::FM_WRITE)
	{
		printfLog("+ ERROR: Can't write to a file opened in read mode.\n");
		return 0;
	}

	// Does that fit?
	if(pointer + numBytes > bufferSize)
	{
		// No - the buffer has to grow.
		uint newBufferSize = pointer + numBytes;
		char* p_newData = new char[newBufferSize];
		memcpy(p_newData, p_data, bufferSize);
		delete[] p_data;
		p_data = p_newData;
		bufferSize = newBufferSize;
	}

	// copy the data
	memcpy(p_data + pointer, p_src, numBytes);
	pointer += numBytes;

	return numBytes;
}

std::list<std::string> File_Archived::listDirectory()
{
	return directory;
}

bool File_Archived::seek(uint newReadPointer)
{
	if(mode != FileSystem::FM_READ)
	{
		printfLog("+ ERROR: Can't seek in a file opened in write mode.\n");
		return false;
	}

	if(newReadPointer > size) return false;
	else
	{
		pointer = newReadPointer;
		eof = pointer >= size;
		return true;
	}
}

bool File_Archived::finish()
{
	if(!outArchive) return false;
	if(mode != FileSystem::FM_WRITE) return false;

	// compute the checksum
	uLong crc = crc32(0, 0, 0);
	crc = crc32(crc, reinterpret_cast<Bytef*>(p_data), pointer);

	// create the new object
	int r = zipOpenNewFileInZip3(outArchive,
								 objectName.c_str(),
								 0, 0, 0, 0, 0, 0,
								 Z_DEFLATED, 9, 0, -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY,
								 password.empty() ? 0 : password.c_str(), crc);
	if(r != ZIP_OK)
	{
		printfLog("+ ERROR: Could not create new file \"%s\" in archive \"%s\" (Error: %d).\n",
				  objectName.c_str(),
				  archiveFilename.c_str(),
				  r);
		zipClose(outArchive, 0);
		outArchive = 0;
		return false;
	}

	// write it
	r = zipWriteInFileInZip(outArchive, p_data, pointer);
	if(r != ZIP_OK)
	{
		printfLog("+ ERROR: Could not write to new file \"%s\" in archive \"%s\" (Error: %d).\n",
				  objectName.c_str(),
				  archiveFilename.c_str(),
				  r);
		zipCloseFileInZip(outArchive);
		zipClose(outArchive, 0);
		outArchive = 0;
		return false;
	}

	// For a member as small as progress.xml these two are where the bytes
	// reach the disk - the stdio flush and the fclose inside zipClose - so a
	// full disk shows up here and nowhere else. Closed on the failures above
	// too: an archive left open has no central directory.
	const int closedMember = zipCloseFileInZip(outArchive);
	const int closedArchive = zipClose(outArchive, 0);
	outArchive = 0;
	if(closedMember != ZIP_OK || closedArchive != ZIP_OK)
	{
		printfLog("+ ERROR: Could not finish file \"%s\" in archive \"%s\" (Error: %d, %d).\n",
				  objectName.c_str(),
				  archiveFilename.c_str(),
				  closedMember, closedArchive);
		return false;
	}

	return true;
}

#pragma pack(push, 1)

int File_Archived::deleteArchivedFile(const std::string& archiveFilename,
									  const std::string& objectName)
{
	int result = 0;

	struct LocalFileHeader
	{
		uint signature;
		ushort versionNeeded;
		ushort flags;
		ushort method;
		ushort modTime;
		ushort modDate;
		uint crc;
		uint compressedSize;
		uint uncompressedSize;
		ushort filenameLength;
		ushort extraFieldLength;
	};

	struct EndOfCentralDirectory
	{
		uint signature;
		ushort thisDisk;
		ushort centralRecordDisk;
		ushort entriesOnThisDisk;
		ushort totalEntries;
		uint centralDirectorySize;
		uint centralDirectoryOffset;
		ushort globalCommentLength;
	};

	struct CentralDirectoryEntry
	{
		uint signature;
		ushort versionMadeBy;
		ushort versionNeeded;
		ushort flags;
		ushort method;
		ushort modTime;
		ushort modDate;
		uint crc;
		uint compressedSize;
		uint uncompressedSize;
		ushort filenameLength;
		ushort extraFieldLength;
		ushort commentLength;
		ushort diskNumber;
		ushort intAttribs;
		uint extAttribs;
		uint localHeaderOffset;
	};

	// The survivors are copied into a side file, which replaces the archive
	// at the end. Failing to open either file, to parse a record or to write
	// the side file whole - a full disk - leaves the archive untouched and
	// reports -2.
	const std::string tempFilename = archiveFilename + "_";
	FILE* p_in = fopen(archiveFilename.c_str(), "rb");
	if(!p_in)
	{
		printfLog("+ ERROR: Could not open archive \"%s\" for rewriting.\n",
				  archiveFilename.c_str());
		return -2;
	}
	FILE* p_out = fopen(tempFilename.c_str(), "wb");
	if(!p_out)
	{
		printfLog("+ ERROR: Could not create \"%s\".\n", tempFilename.c_str());
		fclose(p_in);
		return -2;
	}

	// find the central directory
	while(true)
	{
		uint signature = 0;
		uint pos = ftell(p_in);
		const bool read = fread(&signature, 1, 4, p_in) == 4;
		fseek(p_in, pos, SEEK_SET);

		if(read && signature == 0x04034B50)
		{
			LocalFileHeader lfh;
			fread(&lfh, 1, sizeof(lfh), p_in);
			fseek(p_in, lfh.filenameLength + lfh.extraFieldLength + lfh.compressedSize, SEEK_CUR);
		}
		else if(read && signature == 0x02014B50)
		{
			CentralDirectoryEntry cde;
			fread(&cde, 1, sizeof(cde), p_in);
			fseek(p_in, cde.filenameLength + cde.extraFieldLength + cde.commentLength, SEEK_CUR);
		}
		else if(read && signature == 0x06054B50)
		{
			// That is what we were looking for.
			break;
		}
		else
		{
			// A record this scan does not know - zip64, a data descriptor
			// behind an entry with flag bit 3 set - or a truncated file.
			printfLog("+ ERROR: Archive \"%s\" has a record that cannot be rewritten.\n",
					  archiveFilename.c_str());
			fclose(p_in);
			fclose(p_out);
			remove(tempFilename.c_str());
			return -2;
		}
	}

	EndOfCentralDirectory ecd, ecdOut;
	fread(&ecd, 1, sizeof(ecd), p_in);
	char* p_globalComment = 0;
	if(ecd.globalCommentLength)
	{
		p_globalComment = new char[ecd.globalCommentLength];
		fread(p_globalComment, 1, ecd.globalCommentLength, p_in);
	}
	ecdOut = ecd;
	bool written = true;

	fseek(p_in, ecd.centralDirectoryOffset, SEEK_SET);

	std::vector<CentralDirectoryEntry> cdOut;
	std::vector<char*> filenameOut, extraFieldOut, commentOut;

	// read and write the entries
	for(uint i = 0; i < ecd.totalEntries; i++)
	{
		CentralDirectoryEntry cde, cdeOut;
		fread(&cde, 1, sizeof(cde), p_in);
		cdeOut = cde;
		char* p_filename = new char[cde.filenameLength + 1];
		fread(p_filename, 1, cde.filenameLength, p_in);
		p_filename[cde.filenameLength] = 0;

		char* p_extraField = 0;
		if(cde.extraFieldLength)
		{
			p_extraField = new char[cde.extraFieldLength];
			fread(p_extraField, 1, cde.extraFieldLength, p_in);
		}
		char* p_comment = 0;
		if(cde.commentLength)
		{
			p_comment = new char[cde.commentLength];
			fread(p_comment, 1, cde.commentLength, p_in);
		}

		// remember where to carry on afterwards
		uint nextCDE = ftell(p_in);

		// Does the filename match the one that is to be deleted?
		if(equalsNoCase(objectName.c_str(), p_filename))
		{
			ecdOut.entriesOnThisDisk--;
			ecdOut.totalEntries--;
			ecdOut.centralDirectorySize -= sizeof(cde) + cde.filenameLength + cde.extraFieldLength + cde.commentLength;
			result = 1;

			delete[] p_filename;
			delete[] p_extraField;
			delete[] p_comment;
		}
		else
		{
			// Copy the whole local record byte for byte: header, filename,
			// extra field and data. It must not be rebuilt from the central
			// directory's copies: a zip writer may put a different extra field
			// in each, and the local header's lengths would then read past the
			// end of the shorter buffer.
			//
			// The data size comes from the central directory, which carries
			// the true one even where the local header does not. A trailing
			// data descriptor is not carried across; nothing this game packs
			// sets the flag that calls for one, which is also why pack.sh uses
			// 7za rather than Info-ZIP.
			LocalFileHeader lfh;
			fseek(p_in, cde.localHeaderOffset, SEEK_SET);
			fread(&lfh, 1, sizeof(lfh), p_in);

			const uint recordSize = static_cast<uint>(sizeof(lfh))
									+ lfh.filenameLength
									+ lfh.extraFieldLength
									+ cde.compressedSize;

			char* p_record = new char[recordSize];
			fseek(p_in, cde.localHeaderOffset, SEEK_SET);
			fread(p_record, 1, recordSize, p_in);

			cdeOut.localHeaderOffset = ftell(p_out);
			written = fwrite(p_record, 1, recordSize, p_out) == recordSize && written;
			delete[] p_record;

			// remember the entry for the central directory
			cdOut.push_back(cdeOut);
			filenameOut.push_back(p_filename);
			extraFieldOut.push_back(p_extraField);
			commentOut.push_back(p_comment);
		}

		fseek(p_in, nextCDE, SEEK_SET);
	}

	// write the central directory
	ecdOut.centralDirectoryOffset = ftell(p_out);
	for(uint i = 0; i < cdOut.size(); i++)
	{
		CentralDirectoryEntry& cde = cdOut[i];
		written = fwrite(&cde, 1, sizeof(cde), p_out) == sizeof(cde) && written;

		char* p_filename = filenameOut[i];
		char* p_extraField = extraFieldOut[i];
		char* p_comment = commentOut[i];

		written = fwrite(p_filename, 1, cde.filenameLength, p_out) == cde.filenameLength && written;
		if(cde.extraFieldLength) written = fwrite(p_extraField, 1, cde.extraFieldLength, p_out) == cde.extraFieldLength && written;
		if(cde.commentLength) written = fwrite(p_comment, 1, cde.commentLength, p_out) == cde.commentLength && written;

		delete[] p_filename;
		delete[] p_extraField;
		delete[] p_comment;
	}

	// write the end record
	written = fwrite(&ecdOut, 1, sizeof(ecdOut), p_out) == sizeof(ecdOut) && written;
	if(ecdOut.globalCommentLength) written = fwrite(p_globalComment, 1, ecdOut.globalCommentLength, p_out) == ecdOut.globalCommentLength && written;

	delete[] p_globalComment;

	fclose(p_in);
	// stdio's buffer goes out here, and with it the last of a full disk.
	written = fclose(p_out) == 0 && written;

	if(!ecdOut.totalEntries)
	{
		// Nothing is left in it: the archive goes, the side file with it.
		remove(archiveFilename.c_str());
		remove(tempFilename.c_str());
		return -1;
	}

	if(!written)
	{
		printfLog("+ ERROR: Could not write \"%s\"; the archive is left as it was.\n",
				  tempFilename.c_str());
		remove(tempFilename.c_str());
		return -2;
	}

	// The side file takes the archive's place in one step, so that there is
	// never a moment with neither: rename() under POSIX, and under Windows,
	// whose rename() refuses an existing target, MoveFileEx. A replacement
	// that fails leaves the archive as it was.
#ifdef _WIN32
	const bool replaced = MoveFileExA(tempFilename.c_str(), archiveFilename.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
#else
	const bool replaced = rename(tempFilename.c_str(), archiveFilename.c_str()) == 0;
#endif
	if(!replaced)
	{
		printfLog("+ ERROR: Could not replace \"%s\"; the archive is left as it was.\n",
				  archiveFilename.c_str());
		remove(tempFilename.c_str());
		return -2;
	}

	return result;
}

#pragma pack(pop)