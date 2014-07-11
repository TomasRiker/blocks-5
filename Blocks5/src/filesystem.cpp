#include "pch.h"
#include "filesystem.h"
#include "file_real.h"
#include "file_archived.h"

#ifdef _WIN32
#include <Shlobj.h>
#endif

FileSystem::FileSystem()
{
	// die ersten 256 Primzahlen für die Entschlüsselung von Passwörtern berechnen
	generatePrimes(primes, 256);
}

FileSystem::~FileSystem()
{
}

void FileSystem::closeFile(File* p_file)
{
	delete p_file;
}

std::string FileSystem::evalPath(const std::string& path) const
{
	std::string result = evalRelativePath(path, isAbsolutePath(path) ? "" : getCurrentDir());

	// Slashes vereinheitlichen und doppelte Slashes entfernen
	std::string clean;
	bool slash = false;
	for(uint i = 0; i < result.length(); i++)
	{
		if(result[i] == '/' || result[i] == '\\')
		{
			result[i] = '/';
			if(slash) continue;
			slash = true;
		}
		else slash = false;
		clean += result[i];
	}

	return clean;
}

std::string FileSystem::getAppHomeDirectory() const
{
#ifdef _WIN32
	char path[256];
	SHGetFolderPathA(NULL, CSIDL_MYDOCUMENTS, 0, 0, path);
	return std::string(path) + "/Blocks 5/";
#else
#error NOT IMPLEMENTED
#endif
}

std::string FileSystem::getCurrentDir() const
{
	if(dirStack.empty()) return "";
	else return dirStack.top();
}

std::string FileSystem::getPathDirectory(const std::string& path) const
{
	char* p_temp = new char[path.length() + 1];
	strcpy(p_temp, path.c_str());
	uint i = static_cast<uint>(path.length()) - 1;
	while(i && p_temp[i] != '/') i--;
	p_temp[i] = 0;
	std::string r(p_temp);
	delete[] p_temp;
	return r;
}

std::string FileSystem::getPathFilename(const std::string& path) const
{
	char* p_temp = new char[path.length() + 1];
	strcpy(p_temp, path.c_str());
	uint i = static_cast<uint>(path.length() - 1);
	while(i && p_temp[i] != '/') i--;
	std::string r(i ? p_temp + i + 1 : p_temp);
	delete[] p_temp;
	return r;
}

File* FileSystem::openFile(const std::string& filename,
						   FileMode mode)
{
	// Dateipfad untersuchen
	std::string filePath, objectName, password;
	convertPath(evalPath(filename), filePath, objectName, password);

	// Normale Datei oder Archivobjekt?
	File* p_file = 0;
	if(!filePath.empty())
	{
		if(objectName.empty()) p_file = new File_Real(filePath, mode);
		else p_file = new File_Archived(filePath, objectName, password, mode);
	}

	// Ist ein Fehler aufgetreten?
	if(p_file->getError()) return 0;
	else return p_file;
}

bool FileSystem::fileExists(const std::string& filename)
{
	File* p_file = openFile(filename, FM_TEST);
	if(p_file)
	{
		closeFile(p_file);
		return true;
	}
	else return false;
}

bool FileSystem::deleteFile(const std::string& filename)
{
	File* p_file = openFile(filename, FM_DELETE);
	if(p_file)
	{
		closeFile(p_file);
		return true;
	}
	else return false;
}

bool FileSystem::copyFile(const std::string& source,
						  const std::string& dest)
{
	File* p_source = openFile(source, FM_READ);
	if(!p_source) return false;
	uint size = p_source->getSize();
	char* p_buffer = new char[size];
	uint numBytesRead = p_source->read(p_buffer, size);
	closeFile(p_source);

	if(numBytesRead != size)
	{
		delete[] p_buffer;
		return false;
	}

	File* p_dest = openFile(dest, FM_WRITE);
	if(!p_dest)
	{
		delete[] p_buffer;
		return false;
	}

	uint numBytesWritten = p_dest->write(p_buffer, size);
	closeFile(p_dest);
	delete[] p_buffer;

	return numBytesWritten == size;
}

bool FileSystem::createDirectory(const std::string& directory)
{
#ifdef _WIN32
	BOOL result = CreateDirectoryA(directory.c_str(), 0);
	if(!result && GetLastError() == ERROR_ALREADY_EXISTS) return true;
	else return result != 0;
#else
#error NOT IMPLEMENTED
#endif
}

bool FileSystem::deleteDirectory(const std::string& directory)
{
#ifdef _WIN32
	return RemoveDirectoryA(directory.c_str()) != 0;
#else
#error NOT IMPLEMENTED
#endif
}

std::string FileSystem::readStringFromFile(const std::string& filename)
{
	File* p_file = openFile(filename, FM_READ);
	if(!p_file) return "";
	uint size = p_file->getSize();
	if(!size) return "";
	char* p_buffer = new char[size + 1];
	const uint numBytesRead = p_file->read(p_buffer, size);
	p_buffer[min(size, numBytesRead)] = 0;
	const std::string text(p_buffer);
	delete[] p_buffer;
	closeFile(p_file);

	return text;
}

bool FileSystem::writeStringToFile(const std::string& text,
								   const std::string& filename)
{
	File* p_file = openFile(filename, FM_WRITE);
	if(!p_file) return false;
	const uint numBytesToWrite = static_cast<uint>(text.length());
	const uint numBytesWritten = p_file->write(text.c_str(), numBytesToWrite);
	bool r = p_file->finish();
	closeFile(p_file);
	return r && numBytesWritten == numBytesToWrite;
}

void FileSystem::popCurrentDir()
{
	if(!dirStack.empty()) dirStack.pop();
}

void FileSystem::pushCurrentDir(const std::string& dir)
{
	dirStack.push(evalPath(dir + "/"));
}

void FileSystem::convertPath(const std::string& path,
							 std::string& filePath,
							 std::string& objectName,
							 std::string& password) const
{
	std::string temp(path);
	for(uint i = 0; i < temp.length() - 5; i++)
	{
		if(temp.substr(i, 5) == ".zip/")
		{
			// archivierte Datei
			filePath = temp.substr(0, i + 4);
			objectName = temp.substr(i + 5);
			password = "";
			return;
		}
		else if(temp.substr(i, 5) == ".zip<")
		{
			// archivierte Datei mit Klartext-Passwort
			for(uint j = i + 5; j < temp.length(); j++)
			{
				if(temp[j] == '>')
				{
					filePath = temp.substr(0, i + 4);
					objectName = temp.substr(j + 2);
					password = temp.substr(i + 5, j - (i + 5));
					return;
				}
			}
		}
		else if(temp.substr(i, 5) == ".zip[")
		{
			// archivierte Datei mit verschlüsseltem Passwort
			for(uint j = i + 5; j < temp.length(); j++)
			{
				if(temp[j] == ']')
				{
					filePath = temp.substr(0, i + 4);
					objectName = temp.substr(j + 2);
					password = temp.substr(i + 5, j - (i + 5));

					// Passwort entschlüsseln
					char* p_temp = new char[password.length() + 1];
					decryptPassword(password.c_str(), p_temp, primes);
					password = p_temp;
					delete[] p_temp;
					return;
				}
			}
		}
	}

#ifdef WIN32
	for(uint i = 0; i < temp.length(); i++)
	{
		if(temp[i] == '/') temp[i] = '\\';
	}
#endif

	filePath = temp;
	objectName = "";
	password = "";
}

std::string FileSystem::evalRelativePath(const std::string& path,
										 const std::string& basePath) const
{
	std::string result = basePath;
	for(uint i = 0; i < path.length();)
	{
		if(i < path.length() - 2 &&
		   path.substr(i, 3) == "../")
		{
			// eine Ebene höher im Verzeichnisbaum aufsteigen
			if(result.length() <= 1) return "[INVALID]";
			char* p_temp = new char[result.length() + 1];
			strcpy(p_temp, result.c_str());
			uint j = static_cast<uint>(result.length() - 2);
			while(j && p_temp[j] != '/') j--;
			p_temp[j] = 0;
			result = p_temp;
			delete[] p_temp;
			result += '/';
			i += 3;
		}
		else if(i < path.length() - 1 &&
				path.substr(i, 2) == "./")
		{
			// ignorieren
			i += 2;
		}
		else
		{
			// Zeichen anhängen
			result += path[i];
			i++;
		}
	}

	return result;
}

bool FileSystem::isAbsolutePath(const std::string& path) const
{
	if(path.empty()) return false;
	else return path[0] == '/' || (path.length() >= 2 && path[1] == ':');
}

std::list<std::string> FileSystem::listDirectory(const std::string& directory)
{
	std::list<std::string> result;

	File* p_file = openFile(directory, FM_LIST);
	if(p_file)
	{
		result = p_file->listDirectory();
		closeFile(p_file);
	}

	return result;
}