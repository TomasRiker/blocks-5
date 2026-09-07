#ifndef _MANAGER_H
#define _MANAGER_H

/*** Manager class for resources ***/

#include "resource.h"

template<typename T> class Manager : public Singleton<Manager<T> >
{
	friend class Singleton<Manager<T> >;

public:
	void exit()
	{
		// delete every object that is still loaded
		for(typename std::unordered_multimap<std::string, T*>::const_iterator i = items.begin(); i != items.end(); ++i)
		{
#ifdef _DEBUG
			printfLog("> INFO: Resource \"%s\" is being released automatically ...\n",
					  i->first.c_str());
#endif

			delete i->second;
		}

		items.clear();
	}

	T* find(const std::string& filename) const
	{
		typedef std::unordered_multimap<std::string, T*> mapType;
		std::pair<typename mapType::const_iterator, typename mapType::const_iterator> range = items.equal_range(filename);

		// look for the newest resource
		uint newestTimestamp = 0;
		T* p_newestResource = 0;
		for(typename mapType::const_iterator i = range.first; i != range.second; ++i)
		{
			if(!p_newestResource || i->second->getTimestamp() > newestTimestamp)
			{
				newestTimestamp = i->second->getTimestamp();
				p_newestResource = i->second;
			}
		}

		return p_newestResource;
	}

	T* request(const std::string& filename,
			   bool forceReload = false)
	{
		if(!T::forceReload() && !forceReload)
		{
			// object already loaded?
			T* p_resource = find(filename);
			if(p_resource)
			{
				// yes, raise the reference counter and return the loaded object
				p_resource->refCounter++;
				return p_resource;
			}
		}

#ifdef _DEBUG
		printfLog("> INFO: Resource \"%s\" is being loaded ...\n",
				  filename.c_str());
#endif

		// load the object afresh and return it
		T* p_item = new T(filename);
		if(p_item->error)
		{
			printfLog("+ ERROR: Could not load resource \"%s\" (Error: %d).\n",
					  filename.c_str(),
					  p_item->error);
			return 0;
		}
		else
		{
			items.insert(std::pair<std::string, T*>(filename, p_item));
			return p_item;
		}
	}

	void destroy(Resource<T>* p_item)
	{
#ifdef _DEBUG
		printfLog("> INFO: Resource \"%s\" is being released ...\n",
				  p_item->filename.c_str());
#endif

		// delete the object
		std::pair<typename std::unordered_multimap<std::string, T*>::iterator, typename std::unordered_multimap<std::string, T*>::iterator> p = items.equal_range(p_item->filename);
		for(typename std::unordered_multimap<std::string, T*>::iterator i = p.first; i != p.second; ++i)
		{
			if(i->second == p_item)
			{
				// Found!
				items.erase(i);
				break;
			}
		}

		delete p_item;
	}

	unsigned int reload(const std::string& filename = "")
	{
		unsigned int counter = 0;

		typedef std::unordered_multimap<std::string, T*> mapType;
		std::pair<typename mapType::const_iterator, typename mapType::const_iterator> range;

		if(filename.empty())
		{
			// reload every resource
			range.first = items.begin();
			range.second = items.end();
		}
		else
		{
			// reload only the resources with the given filename
			range = items.equal_range(filename);
		}

		for(typename mapType::const_iterator i = range.first; i != range.second; ++i)
		{
#ifdef _DEBUG
			printfLog("> INFO: Resource \"%s\" is being reloaded ...\n",
					  i->second->filename.c_str());
#endif
			i->second->reload();
			counter++;
		}

		return counter;
	}

	const std::unordered_multimap<std::string, T*>& getItems() const
	{
		return items;
	}

private:
	Manager()
	{
	}

	~Manager()
	{
		exit();
	}

	std::unordered_multimap<std::string, T*> items;
};

#endif