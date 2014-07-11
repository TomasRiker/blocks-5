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

int File_RWSeek(SDL_RWops* p_context, int offset, int whence);
int File_RWRead(SDL_RWops* p_context, void* p_ptr, int size, int maxNum);
int File_RWWrite(SDL_RWops* p_context, const void* p_ptr, int size, int num);
int File_RWClose(SDL_RWops* p_context);

int File_OVSeek(void* p_context, ogg_int64_t offset, int whence);
long File_OVTell(void* p_context);
size_t File_OVRead(void* p_ptr, size_t size, size_t nmemb, void* p_context);
int File_OVClose(void* p_context);

#endif