#include "pch.h"
#include "filesystem.h"
#include "file_real.h"
#include "file_archived.h"

#ifdef _WIN32
#include <Shlobj.h>
#endif
#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#endif

FileSystem::FileSystem()
{
	// compute the first 256 prime numbers for decrypting passwords
	generatePrimes(primes, 256);

	// The working directory at startup is the game folder - the game insists
	// on that anyway, because it opens data.zip relative to it. Captured here
	// while it is still true.
	char path[1024] = "";
#ifdef _WIN32
	if(!GetCurrentDirectoryA(sizeof(path), path)) path[0] = 0;
#else
	if(!::getcwd(path, sizeof(path))) path[0] = 0;
#endif
	gameDirectory = path;
	for(std::string::iterator i = gameDirectory.begin(); i != gameDirectory.end(); ++i)
	{
		if(*i == '\\') *i = '/';
	}
	if(gameDirectory.empty() || gameDirectory[gameDirectory.length() - 1] != '/') gameDirectory += '/';
}

namespace
{
	// Files that belong to the player even though they ship with the game.
	// They are meant for the player: the two example levels are there to be
	// opened, changed and saved again under their own name, and the readmes
	// explain the player's own folders.
	//
	// For them the search order is reversed - the user directory first, then
	// the game folder - and they are never shipped in the sense of
	// "untouchable". The two halves belong together: allowing the save while
	// still answering from the game folder would write a file that could never
	// be read back.
	//
	// Hard-written rather than a directory listing: a working tree holds the
	// forty-two campaign source levels beside the examples, and those are
	// nothing of the kind.
	//
	// Only the readmes need copyOnFirstStart, because nothing in the game ever
	// reads them - without the copy they would sit in no folder at all. An
	// example level is in every list with or without a copy, and without one
	// the player gets the new example along with a new version of the game, as
	// long as they have not saved their own. Saving creates it by itself.
	const FileSystem::PlayerFile p_playerFiles[] =
	{
		{ "levels/example01.xml",        false },
		{ "levels/example02.xml",        false },
		{ "levels/readme.txt",           true  },
		{ "levels/campaigns/readme.txt", true  },
		{ "levels/skins/readme.txt",     true  },
		{ "screenshots/readme.txt",      true  },
		{ "videos/readme.txt",           true  },
		{ 0,                             false }
	};
}

const FileSystem::PlayerFile* FileSystem::getPlayerFiles()
{
	return p_playerFiles;
}

bool FileSystem::belongsToPlayer(const std::string& relative) const
{
	// Case-insensitive, the way the file system under Windows handles it too.
	for(const PlayerFile* p_file = p_playerFiles; p_file->p_path; p_file++)
	{
		if(equalsNoCase(relative.c_str(), p_file->p_path)) return true;
	}
	return false;
}

std::string FileSystem::resolveContentPath(const std::string& relative) const
{
	// const_cast because fileExists() opens a file and is therefore not const;
	// what is meant here is still only a question.
	FileSystem* p_this = const_cast<FileSystem*>(this);
	const std::string own(getAppHomeDirectory() + relative);

	// What belongs to the player is looked for in the user directory first;
	// the game folder is then only the template that stands in as long as the
	// player has none of their own.
	if(belongsToPlayer(relative) && p_this->fileExists(own)) return own;

	const std::string shipped(gameDirectory + relative);
	if(p_this->fileExists(shipped)) return shipped;
	return own;
}

bool FileSystem::isShippedContent(const std::string& relative)
{
	if(belongsToPlayer(relative)) return false;
	return fileExists(gameDirectory + relative);
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

	// unify the slashes and remove doubled ones
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
#elif defined(__EMSCRIPTEN__)
	// Mounted by the page as IDBFS to let saved games and the player's own
	// levels survive a reload.
	return "/blocks5_home/";
#else
	// The XDG Base Directory Specification says where an application's mutable
	// data belongs under Linux: $XDG_DATA_HOME, and where that is not set,
	// $HOME/.local/share. The name is written lowercase and without a space
	// because here it is a path component and not a title - unlike
	// "My Documents\Blocks 5", which Windows shows in the file manager.
	if(const char* p_xdg = ::getenv("XDG_DATA_HOME"))
	{
		if(*p_xdg) return std::string(p_xdg) + "/blocks5/";
	}
	if(const char* p_home = ::getenv("HOME"))
	{
		if(*p_home) return std::string(p_home) + "/.local/share/blocks5/";
	}
	// Without HOME only the working directory is left. That is not a good
	// place, but an empty path would be a worse one: the game would then write
	// into the root.
	return "./blocks5_home/";
#endif
}

std::string FileSystem::getCurrentDir() const
{
	if(dirStack.empty()) return "";
	else return dirStack.top();
}

// Both split the path at the last slash. find_last_of and not a hand-written
// index over a buffer of its own: on "" that index underflows, since length()
// is 0 and 0 - 1 as a uint is 0xFFFFFFFF, and the search then reads backwards
// from there past the single reserved byte.
std::string FileSystem::getPathDirectory(const std::string& path) const
{
	const size_t slash = path.find_last_of('/');
	if(slash == std::string::npos) return "";
	return path.substr(0, slash);
}

std::string FileSystem::getPathFilename(const std::string& path) const
{
	const size_t slash = path.find_last_of('/');
	if(slash == std::string::npos) return path;
	return path.substr(slash + 1);
}

File* FileSystem::openFile(const std::string& filename,
						   FileMode mode)
{
	// examine the file path
	std::string filePath, objectName, password;
	convertPath(evalPath(filename), filePath, objectName, password);

	// A normal file or an archive object?
	File* p_file = 0;
	if(!filePath.empty())
	{
		if(objectName.empty()) p_file = new File_Real(filePath, mode);
		else p_file = new File_Archived(filePath, objectName, password, mode);
	}

	// Did an error occur? An empty path leaves p_file at 0: convertPath()
	// answers an empty name with an empty filePath, and the block above then
	// constructs nothing.
	if(!p_file) return 0;
	if(p_file->getError())
	{
		closeFile(p_file);
		return 0;
	}
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

bool FileSystem::renameFile(const std::string& source,
							const std::string& dest)
{
	// Both plain files? An archive member has no name of its own on the disk,
	// so there is nothing for rename() to move.
	std::string sourcePath, sourceObject, sourcePassword;
	std::string destPath, destObject, destPassword;
	convertPath(evalPath(source), sourcePath, sourceObject, sourcePassword);
	convertPath(evalPath(dest), destPath, destObject, destPassword);

	if(!sourcePath.empty() && sourceObject.empty() &&
	   !destPath.empty() && destObject.empty())
	{
		// The destination gives way only after the first attempt has failed:
		// POSIX replaces it in one atomic step, and deleting it beforehand
		// would open a window in which neither name exists. Windows refuses
		// the replacement, which is what the second attempt is for.
		if(rename(sourcePath.c_str(), destPath.c_str()) == 0) return true;
		if(fileExists(dest) && deleteFile(dest) &&
		   rename(sourcePath.c_str(), destPath.c_str()) == 0) return true;
	}

	// The copy has to be complete before the original goes, or a failure
	// halfway would leave neither.
	if(!copyFile(source, dest)) return false;
	return deleteFile(source);
}

bool FileSystem::createDirectory(const std::string& directory)
{
#ifdef _WIN32
	BOOL result = CreateDirectoryA(directory.c_str(), 0);
	if(!result && GetLastError() == ERROR_ALREADY_EXISTS) return true;
	else return result != 0;
#else
	// Create the parent directories along with it: under Windows the user
	// directory appears in "My Documents", which always exists already, while
	// under Linux it lands in ~/.local/share, where even the share may be
	// missing.
	std::string path;
	for(size_t i = 0; i <= directory.length(); i++)
	{
		if(i == directory.length() || directory[i] == '/')
		{
			if(!path.empty() && ::mkdir(path.c_str(), 0755) != 0 && errno != EEXIST) return false;
		}
		if(i < directory.length()) path += directory[i];
	}
	return true;
#endif
}

bool FileSystem::deleteDirectory(const std::string& directory)
{
#ifdef _WIN32
	return RemoveDirectoryA(directory.c_str()) != 0;
#else
	return ::rmdir(directory.c_str()) == 0;
#endif
}

std::string FileSystem::readStringFromFile(const std::string& filename)
{
	File* p_file = openFile(filename, FM_READ);
	if(!p_file) return "";
	uint size = p_file->getSize();
	if(!size)
	{
		closeFile(p_file);
		return "";
	}
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
	// i + 5 <= length, not i < length - 5. On a path of fewer than five
	// characters the subtraction underflows: the loop then runs to the end of
	// the string and substr throws as soon as pos is greater than the length.
	// Incidentally this checks the last possible position too, and a path
	// ending exactly in ".zip/" is therefore recognised.
	for(uint i = 0; i + 5 <= temp.length(); i++)
	{
		if(temp.substr(i, 5) == ".zip/")
		{
			// archived file
			filePath = temp.substr(0, i + 4);
			objectName = temp.substr(i + 5);
			password = "";
			return;
		}
		else if(temp.substr(i, 5) == ".zip<")
		{
			// archived file with a plaintext password
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
			// archived file with an encrypted password
			for(uint j = i + 5; j < temp.length(); j++)
			{
				if(temp[j] == ']')
				{
					filePath = temp.substr(0, i + 4);
					objectName = temp.substr(j + 2);
					password = temp.substr(i + 5, j - (i + 5));

					// decrypt the password
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
			// go up one level in the directory tree
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
			// ignore it
			i += 2;
		}
		else
		{
			// append the character
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