#ifndef _MANAGER_H
#define _MANAGER_H

/*** Manager-Klasse für Ressourcen ***/

#include "resource.h"

template<typename T> class Manager : public Singleton<Manager<T> >
{
	friend class Singleton<Manager<T> >;

public:
	void exit()
	{
		// alle noch geladenen Objekte löschen
		for(stdext::hash_multimap<std::string, T*>::const_iterator i = items.begin(); i != items.end(); ++i)
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
		typedef stdext::hash_multimap<std::string, T*> mapType;
		std::pair<mapType::const_iterator, mapType::const_iterator> range = items.equal_range(filename);

		// neueste Ressource suchen
		uint newestTimestamp = 0;
		T* p_newestResource = 0;
		for(mapType::const_iterator i = range.first; i != range.second; ++i)
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
			// Objekt schon geladen?
			T* p_resource = find(filename);
			if(p_resource)
			{
				// ja, Referenzzähler erhöhen und geladenes Objekt liefern
				p_resource->refCounter++;
				return p_resource;
			}
		}

#ifdef _DEBUG
		printfLog("> INFO: Resource \"%s\" is being loaded ...\n",
				  filename.c_str());
#endif

		// Objekt neu laden und zurückliefern
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

		// Objekt löschen
		std::pair<stdext::hash_multimap<std::string, T*>::iterator, stdext::hash_multimap<std::string, T*>::iterator> p = items.equal_range(p_item->filename);
		for(stdext::hash_multimap<std::string, T*>::iterator i = p.first; i != p.second; ++i)
		{
			if(i->second == p_item)
			{
				// Gefunden!
				items.erase(i);
				break;
			}
		}

		delete p_item;
	}

	unsigned int reload(const std::string& filename = "")
	{
		unsigned int counter = 0;

		typedef stdext::hash_multimap<std::string, T*> mapType;
		std::pair<mapType::const_iterator, mapType::const_iterator> range;

		if(filename.empty())
		{
			// alle Ressourcen neu laden
			range.first = items.begin();
			range.second = items.end();
		}
		else
		{
			// nur die Ressourcen mit dem angegebenen Dateinamen neu laden
			range = items.equal_range(filename);
		}

		for(mapType::const_iterator i = range.first; i != range.second; ++i)
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

	const stdext::hash_multimap<std::string, T*>& getItems() const
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

	stdext::hash_multimap<std::string, T*> items;
};

#endif