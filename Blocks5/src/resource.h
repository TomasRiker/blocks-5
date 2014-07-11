#ifndef _RESOURCE_H
#define _RESOURCE_H

/*** Ressourcenklasse ***/

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
			// Ressource abbauen
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