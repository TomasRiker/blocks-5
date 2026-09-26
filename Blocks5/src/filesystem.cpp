#include "pch.h"
#include "filesystem.h"
#include "file_real.h"
#include "file_archived.h"
#include <new>

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

	// The working directory at startup is the game folder, since the game
	// opens data.zip relative to it; captured while that is still true.
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
	// Files that ship with the game but belong to the player: examples to
	// change and save, readmes about the player's folders. They are looked for
	// in the user directory first and never count as shipped - the two go
	// together, or a save could never be read back. Written out, because a
	// working tree holds the forty-two campaign sources beside the examples.
	// Only the readmes are copied on the first start, since nothing in the
	// game reads them; an example stays the newest shipped one until the
	// player saves their own.
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
	// Case-insensitive, as the file system under Windows is.
	for(const PlayerFile* p_file = p_playerFiles; p_file->p_path; p_file++)
	{
		if(equalsNoCase(relative.c_str(), p_file->p_path)) return true;
	}
	return false;
}

std::string FileSystem::resolveContentPath(const std::string& relative) const
{
	// fileExists() opens a file and so is not const, but it is still only a
	// question.
	FileSystem* p_this = const_cast<FileSystem*>(this);
	const std::string own(getAppHomeDirectory() + relative);

	// A player's file: the game folder's copy stands in only while the player
	// has none of their own.
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

	// Unify the slashes and remove doubled ones - but not a leading pair,
	// which is a network path under Windows (\\server\share, where a
	// redirected Documents folder can lie) rather than a doubling.
	std::string clean;
	bool slash = false;
	for(uint i = 0; i < result.length(); i++)
	{
		if(result[i] == '/' || result[i] == '\\')
		{
			result[i] = '/';
			if(slash && i > 1) continue;
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
	// MAX_PATH is the size the call is specified for. Without a Documents
	// folder, a folder beside the game, as under Linux without HOME.
	char path[MAX_PATH];
	if(FAILED(SHGetFolderPathA(NULL, CSIDL_MYDOCUMENTS, 0, 0, path))) return gameDirectory + "Blocks 5/";
	return std::string(path) + "/Blocks 5/";
#elif defined(__EMSCRIPTEN__)
	// Mounted by the page as IDBFS, so that saved games and the player's own
	// levels survive a reload.
	return "/blocks5_home/";
#else
	// The XDG Base Directory Specification: $XDG_DATA_HOME, or where that is
	// not set, $HOME/.local/share. Lowercase and without a space, because here
	// the name is a path component and not a title shown in a file manager.
	// A relative one is to be ignored, the specification says, and here it
	// would do harm: once main() has mounted data.zip as the current
	// directory, every relative path is resolved inside the archive, and the
	// player's files would be written into it.
	if(const char* p_xdg = ::getenv("XDG_DATA_HOME"))
	{
		if(*p_xdg == '/') return std::string(p_xdg) + "/blocks5/";
	}
	if(const char* p_home = ::getenv("HOME"))
	{
		if(*p_home == '/') return std::string(p_home) + "/.local/share/blocks5/";
	}
	// Without a usable HOME, a folder beside the game, and absolute for the
	// same reason.
	return gameDirectory + "blocks5_home/";
#endif
}

std::string FileSystem::getCurrentDir() const
{
	if(dirStack.empty()) return "";
	else return dirStack.top();
}

// Both split the path at the last slash.
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

	// Did an error occur? An empty path gets an empty filePath from
	// convertPath() and constructs nothing at all.
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

	// finish() and not only the write: a member of an archive is compressed
	// and written there, and a plain file reaches the disk there.
	const uint numBytesWritten = p_dest->write(p_buffer, size);
	const bool finished = p_dest->finish();
	closeFile(p_dest);
	delete[] p_buffer;

	return finished && numBytesWritten == size;
}

bool FileSystem::renameFile(const std::string& source,
							const std::string& dest)
{
	// Both plain files? An archive member has no name on the disk for
	// rename() to move.
	std::string sourcePath, sourceObject, sourcePassword;
	std::string destPath, destObject, destPassword;
	convertPath(evalPath(source), sourcePath, sourceObject, sourcePassword);
	convertPath(evalPath(dest), destPath, destObject, destPassword);

	if(!sourcePath.empty() && sourceObject.empty() &&
	   !destPath.empty() && destObject.empty())
	{
		// The destination is replaced in one step and never deleted first,
		// which would open a window in which neither name exists: rename()
		// does that under POSIX, and under Windows, whose rename() refuses an
		// existing destination, MoveFileEx.
#ifdef _WIN32
		if(MoveFileExA(sourcePath.c_str(), destPath.c_str(), MOVEFILE_REPLACE_EXISTING)) return true;
#else
		if(rename(sourcePath.c_str(), destPath.c_str()) == 0) return true;
#endif
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
	// The parents along with it: under Windows the user directory lies in
	// "My Documents", which always exists, but under Linux even the "share"
	// of ~/.local/share may be missing.
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
	// Without the memory the file reads as empty: nothing up the stack
	// catches a throwing allocation.
	char* p_buffer = new(std::nothrow) char[size + 1];
	if(!p_buffer)
	{
		printfLog("+ ERROR: No memory to read \"%s\" (%u bytes).\n", filename.c_str(), size);
		closeFile(p_file);
		return "";
	}
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
	// i + 5 <= length, not i < length - 5, which underflows on a path shorter
	// than five characters; this form also tests the last position, so a path
	// ending exactly in ".zip/" is recognised. ".zip" in any case: an archive
	// picked in a file dialog can be called CAMPAIGN.ZIP.
	for(uint i = 0; i + 5 <= temp.length(); i++)
	{
		if(!equalsNoCase(temp.substr(i, 4).c_str(), ".zip")) continue;
		const char marker = temp[i + 4];
		if(marker == '/')
		{
			// archived file
			filePath = temp.substr(0, i + 4);
			objectName = temp.substr(i + 5);
			password = "";
			return;
		}
		else if(marker == '<')
		{
			// archived file with a plaintext password
			for(uint j = i + 5; j < temp.length(); j++)
			{
				if(temp[j] == '>')
				{
					// Only "archive.zip<password>/member" is this syntax. An
					// imported file is named by whoever made it, and one like
					// "a.zip[1]" ends on the bracket with no member after it:
					// such a path is a plain name.
					if(j + 1 >= temp.length() || temp[j + 1] != '/') break;
					filePath = temp.substr(0, i + 4);
					objectName = temp.substr(j + 2);
					password = temp.substr(i + 5, j - (i + 5));
					return;
				}
			}
		}
		else if(marker == '[')
		{
			// archived file with an encrypted password
			for(uint j = i + 5; j < temp.length(); j++)
			{
				if(temp[j] == ']')
				{
					// as with the plaintext form above
					if(j + 1 >= temp.length() || temp[j + 1] != '/') break;
					filePath = temp.substr(0, i + 4);
					objectName = temp.substr(j + 2);

					// Text that decrypts to nothing leaves the password empty:
					// a member stored without one still opens, any other fails
					// as under a wrong password.
					decryptPassword(temp.substr(i + 5, j - (i + 5)), password, primes);
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
		if(i + 3 <= path.length() &&
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
		else if(i + 2 <= path.length() &&
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
	else return path[0] == '/' || path[0] == '\\' || (path.length() >= 2 && path[1] == ':');
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