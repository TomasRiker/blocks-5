#include "pch.h"
#include "file_archived.h"
#include "filesystem.h"
#include <zip.h>
#include <unzip.h>

File_Archived::File_Archived(const std::string& archiveFilename,
							 const std::string& objectName,
							 const std::string& password,
							 int mode): File(mode)
{
	p_data = 0;
	pointer = 0;
	size = 0;
	eof = false;

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
		// Archiv öffnen
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
			// Objekt finden
			int r = unzLocateFile(archive, objectName.c_str(), 0);
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
			// Dateien auflisten
			int r = unzGoToFirstFile(archive);
			while(r == UNZ_OK)
			{
				unz_file_info info;
				char temp[256] = "";
				unzGetCurrentFileInfo(archive, &info, temp, 256, 0, 0, 0, 0);
				std::string filename(temp, info.size_filename);
				if(filename.find_first_of('/') == std::string::npos) directory.push_back(filename);
				r = unzGoToNextFile(archive);
			}
		}

		if(testMode || listMode)
		{
			unzClose(archive);
			return;
		}

		// Dateiinformationen abfragen
		unz_file_info info;
		unzGetCurrentFileInfo(archive, &info, 0, 0, 0, 0, 0, 0);

		// Speicher reservieren
		size = info.uncompressed_size;
		p_data = new char[size];

		// Objekt einlesen
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

		// Existiert das Archiv schon?
		FILE* p_file = fopen(archiveFilename.c_str(), "rb");
		bool archiveExists = p_file ? true : false;
		if(p_file) fclose(p_file);

		bool objectExists = false;
		if(archiveExists)
		{
			// Existiert die archivierte Datei schon?
			unzFile temp = unzOpen(archiveFilename.c_str());
			if(temp)
			{
				int r = unzLocateFile(temp, objectName.c_str(), 0);
				if(r == UNZ_OK) objectExists = true;
				unzClose(temp);
			}

			if(objectExists)
			{
				// Objekt löschen
				int r = deleteArchivedFile(archiveFilename, objectName);
				if(r == -1) archiveExists = false;
			}
		}

		// Archiv zum Anhängen von Dateien öffnen
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
		// Objekt löschen
		deleteArchivedFile(archiveFilename, objectName);
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

	// Passt das?
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

	// Passt das?
	if(pointer + numBytes > bufferSize)
	{
		// Nein - Puffer muss vergrößert werden!
		uint newBufferSize = pointer + numBytes;
		char* p_newData = new char[newBufferSize];
		memcpy(p_newData, p_data, bufferSize);
		delete[] p_data;
		p_data = p_newData;
		bufferSize = newBufferSize;
	}

	// Daten kopieren
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

	// Prüfsumme berechnen
    uLong crc = crc32(0, 0, 0);
	crc = crc32(crc, reinterpret_cast<Bytef*>(p_data), pointer);

	// neues Objekt anlegen
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
		return false;
	}

	// schreiben
	r = zipWriteInFileInZip(outArchive, p_data, pointer);
	if(r != ZIP_OK)
	{
		printfLog("+ ERROR: Could not write to new file \"%s\" in archive \"%s\" (Error: %d).\n",
				  objectName.c_str(),
				  archiveFilename.c_str(),
				  r);
		return false;
	}

	zipCloseFileInZip(outArchive);
	zipClose(outArchive, 0);
	outArchive = 0;

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

	FILE* p_in = fopen(archiveFilename.c_str(), "rb");
	FILE* p_out = fopen((archiveFilename + "_").c_str(), "wb");

	// zentrales Verzeichnis suchen
	while(true)
	{
		uint signature;
		uint pos = ftell(p_in);
		fread(&signature, 1, 4, p_in);
		fseek(p_in, pos, SEEK_SET);

		if(signature == 0x04034B50)
		{
			LocalFileHeader lfh;
			fread(&lfh, 1, sizeof(lfh), p_in);
			fseek(p_in, lfh.filenameLength + lfh.extraFieldLength + lfh.compressedSize, SEEK_CUR);
		}
		else if(signature == 0x02014B50)
		{
			CentralDirectoryEntry cde;
			fread(&cde, 1, sizeof(cde), p_in);
			fseek(p_in, cde.filenameLength + cde.extraFieldLength + cde.commentLength, SEEK_CUR);
		}
		else if(signature == 0x06054B50)
		{
			// Danach haben wir gesucht!
			break;
		}
		else
		{
			return false;
		}
	}

	EndOfCentralDirectory ecd, ecdOut;
	fread(&ecd, 1, sizeof(ecd), p_in);
	char* p_globalComment = 0;
	if(ecd.globalCommentLength) p_globalComment = new char[ecd.globalCommentLength];
	ecdOut = ecd;

	fseek(p_in, ecd.centralDirectoryOffset, SEEK_SET);

	std::vector<CentralDirectoryEntry> cdOut;
	std::vector<char*> filenameOut, extraFieldOut, commentOut;

	// Einträge lesen und schreiben
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

		// merken, wo es nachher weitergeht
		uint nextCDE = ftell(p_in);

		// Stimmt der Dateiname mit dem zu löschenden Dateinamen überein?
		if(!_stricmp(objectName.c_str(), p_filename))
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
			// Diese Datei soll kopiert werden. Zuerst lesen wir ihren lokalen Header.
			LocalFileHeader lfh;
			fseek(p_in, cde.localHeaderOffset, SEEK_SET);
			fread(&lfh, sizeof(lfh), 1, p_in);

			// Dateiname und Extrafeld überspringen
			fseek(p_in, lfh.filenameLength + lfh.extraFieldLength, SEEK_CUR);

			// Daten lesen
			char* p_fileData = new char[lfh.compressedSize];
			fread(p_fileData, 1, lfh.compressedSize, p_in);

			// lokalen Header schreiben
			cdeOut.localHeaderOffset = ftell(p_out);
			fwrite(&lfh, 1, sizeof(lfh), p_out);
			fwrite(p_filename, 1, lfh.filenameLength, p_out);
			if(lfh.extraFieldLength) fwrite(p_extraField, 1, lfh.extraFieldLength, p_out);
			fwrite(p_fileData, 1, lfh.compressedSize, p_out);
			delete[] p_fileData;

			// Eintrag für das zentrale Verzeichnis merken
			cdOut.push_back(cdeOut);
			filenameOut.push_back(p_filename);
			extraFieldOut.push_back(p_extraField);
			commentOut.push_back(p_comment);
		}

		fseek(p_in, nextCDE, SEEK_SET);
	}

	// zentrales Verzeichnis schreiben
	ecdOut.centralDirectoryOffset = ftell(p_out);
	for(uint i = 0; i < cdOut.size(); i++)
	{
		CentralDirectoryEntry& cde = cdOut[i];
		fwrite(&cde, 1, sizeof(cde), p_out);

		char* p_filename = filenameOut[i];
		char* p_extraField = extraFieldOut[i];
		char* p_comment = commentOut[i];

		fwrite(p_filename, 1, cde.filenameLength, p_out);
		if(cde.extraFieldLength) fwrite(p_extraField, 1, cde.extraFieldLength, p_out);
		if(cde.commentLength) fwrite(p_comment, 1, cde.commentLength, p_out);

		delete[] p_filename;
		delete[] p_extraField;
		delete[] p_comment;
	}

	// Ende schreiben
	fwrite(&ecdOut, 1, sizeof(ecdOut), p_out);
	if(ecdOut.globalCommentLength) fwrite(p_globalComment, 1, ecdOut.globalCommentLength, p_out);

	delete[] p_globalComment;

	fclose(p_in);
	fclose(p_out);

	// altes Archiv löschen
	remove(archiveFilename.c_str());

	if(ecdOut.totalEntries)
	{
		// neue Datei umbenennen
		rename((archiveFilename + "_").c_str(), archiveFilename.c_str());
	}
	else
	{
		// neue Datei löschen
		remove((archiveFilename + "_").c_str());
		result = -1;
	}

	return result;
}

#pragma pack(pop)