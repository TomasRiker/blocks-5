#ifndef _RESOURCE_H
#define _RESOURCE_H

/*** Resource class ***/

template<typename T> class Manager;

template<typename T> class Resource
{
	friend class Manager<T>;

public:
	void addRef()
	{
		refCounter++;
	}

	void release()
	{
		if(!--refCounter)
		{
			// tear the resource down
			Manager<T>::inst().destroy(this);
		}
	}

	virtual void reload()
	{
	}

	const std::string& getFilename() const
	{
		return filename;
	}

	unsigned int getTimestamp() const
	{
		return timestamp;
	}

	// A request for a resource that is already loaded carries options too.
	// Only Texture has any, and it hides this with one that reloads when a
	// later request asks for more (tiling, a texture of its own). The call
	// is resolved on T*, so this empty one costs nothing anywhere else.
	void reuseWithOptions(int) {}

protected:
	Resource(const std::string& filename) : filename(filename)
	{
		refCounter = 1;
		error = 0;
		timestamp = SDL_GetTicks();
	}

	virtual ~Resource()
	{
	}

	int refCounter;
	const std::string filename;
	int error;
	unsigned int timestamp;
};

#endif