#ifndef _FILE_H
#define _FILE_H

/*** Basisklasse für Dateien des virtuellen Dateisystems ***/

class File
{
	friend class FileSystem;

public:
	virtual uint read(void* p_dest, uint numBytes);
	virtual uint write(const void* p_src, uint numBytes);
	virtual std::list<std::string> listDirectory();
	virtual bool finish();
	virtual bool seek(uint newReadPointer);
	virtual uint tell() const;
	virtual uint getSize() const;
	virtual bool isEOF() const;

	int getError() const;
	int getMode() const;
	SDL_RWops* getRWOps();
	ov_callbacks getOVCallbacks();

protected:
	File(int mode);
	virtual ~File();

	int error;
	int mode;
};

#ifdef __EMSCRIPTEN__
typedef long   B5RWOff;   // emscripten ships SDL2-shaped RWops callbacks
typedef size_t B5RWSize;
#else
typedef int B5RWOff;
typedef int B5RWSize;
#endif
B5RWOff File_RWSeek(SDL_RWops* p_context, B5RWOff offset, int whence);
B5RWSize File_RWRead(SDL_RWops* p_context, void* p_ptr, B5RWSize size, B5RWSize maxNum);
B5RWSize File_RWWrite(SDL_RWops* p_context, const void* p_ptr, B5RWSize size, B5RWSize num);
int File_RWClose(SDL_RWops* p_context);

int File_OVSeek(void* p_context, ogg_int64_t offset, int whence);
long File_OVTell(void* p_context);
size_t File_OVRead(void* p_ptr, size_t size, size_t nmemb, void* p_context);
int File_OVClose(void* p_context);

#endif