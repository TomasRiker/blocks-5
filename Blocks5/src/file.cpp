#include "pch.h"
#include "file.h"
#include "filesystem.h"

File::File(int mode)
{
	error = 0;
	this->mode = mode;
}

File::~File()
{
}

bool File::isEOF() const
{
	return true;
}

uint File::getSize() const
{
	return 0;
}

uint File::tell() const
{
	return 0;
}

uint File::read(void* p_dest,
				uint numBytes)
{
	return 0;
}

uint File::write(const void* p_src,
				 uint numBytes)
{
	return 0;
}

std::list<std::string> File::listDirectory()
{
	std::list<std::string> result;
	return result;
}

bool File::seek(uint newReadPointer)
{
	return false;
}

bool File::finish()
{
	return true;
}

int File::getError() const
{
	return error;
}

int File::getMode() const
{
	return mode;
}

SDL_RWops* File::getRWOps()
{
	SDL_RWops* p_rwOps = SDL_AllocRW();
	p_rwOps->read = File_RWRead;
	p_rwOps->write = File_RWWrite;
	p_rwOps->seek = File_RWSeek;
	p_rwOps->close = File_RWClose;
	p_rwOps->hidden.unknown.data1 = this;
	return p_rwOps;
}

ov_callbacks File::getOVCallbacks()
{
	ov_callbacks callbacks;
	callbacks.seek_func = File_OVSeek;
	callbacks.tell_func = File_OVTell;
	callbacks.read_func = File_OVRead;
	callbacks.close_func = File_OVClose;
	return callbacks;
}

int File_RWSeek(SDL_RWops* p_context,
				int offset,
				int whence)
{
	File* p_file = static_cast<File*>(p_context->hidden.unknown.data1);
	switch(whence)
	{
	case SEEK_SET:
		p_file->seek(offset);
		break;
	case SEEK_CUR:
		p_file->seek(p_file->tell() + offset);
		break;
	case SEEK_END:
		p_file->seek(p_file->getSize() + offset);
		break;
	}

	return p_file->tell();
}

int File_RWRead(SDL_RWops* p_context,
				void* p_ptr,
				int size,
				int maxNum)
{
	File* p_file = static_cast<File*>(p_context->hidden.unknown.data1);
    uint numBytesToRead = size * maxNum;
	uint numBytesRead = p_file->read(p_ptr, numBytesToRead);
	return numBytesRead / size;
}

int File_RWWrite(SDL_RWops* p_context,
				 const void* p_ptr,
				 int size,
				 int num)
{
    return 0;
}

int File_RWClose(SDL_RWops* p_context)
{
	File* p_file = static_cast<File*>(p_context->hidden.unknown.data1);
    SDL_FreeRW(p_context);
	FileSystem::inst().closeFile(p_file);

    return 0;
}

int File_OVSeek(void* p_context,
				ogg_int64_t offset,
				int whence)
{
	File* p_file = static_cast<File*>(p_context);
	switch(whence)
	{
	case SEEK_SET:
		p_file->seek(static_cast<int>(offset));
		break;
	case SEEK_CUR:
		p_file->seek(p_file->tell() + static_cast<int>(offset));
		break;
	case SEEK_END:
		p_file->seek(p_file->getSize() + static_cast<int>(offset));
		break;
	}

	return 0;
}

long File_OVTell(void* p_context)
{
	File* p_file = static_cast<File*>(p_context);
	return p_file->tell();
}

size_t File_OVRead(void* p_ptr,
				   size_t size,
				   size_t nmemb,
				   void* p_context)
{
	File* p_file = static_cast<File*>(p_context);
	uint numBytesToRead = static_cast<uint>(size * nmemb);
	uint numBytesRead = p_file->read(p_ptr, numBytesToRead);
	return numBytesRead / size;
}

int File_OVClose(void* p_context)
{
	File* p_file = static_cast<File*>(p_context);
	FileSystem::inst().closeFile(p_file);
	return 0;
}