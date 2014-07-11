#ifndef _PARAMETERBLOCK_H
#define _PARAMETERBLOCK_H

/*** Klasse für eine Parameter-Sammlung ***/

class ParameterBlock
{
public:
	ParameterBlock()
	{
	}

	ParameterBlock(const ParameterBlock& rhs)
	{
		// alle Parameter kopieren
		for(stdext::hash_map<std::string, GenericParameterContainer*>::const_iterator i = rhs.values.begin(); i != rhs.values.end(); ++i)
		{
			values[i->first] = i->second->copy();
		}
	}

	~ParameterBlock()
	{
		clear();
	}

	ParameterBlock& operator = (const ParameterBlock& rhs)
	{
		clear();

		// alle Parameter kopieren
		for(stdext::hash_map<std::string, GenericParameterContainer*>::const_iterator i = rhs.values.begin(); i != rhs.values.end(); ++i)
		{
			values[i->first] = i->second->copy();
		}

		return *this;
	}

	template<typename T> void set(const std::string& key,
								  const T& value)
	{
		stdext::hash_map<std::string, GenericParameterContainer*>::iterator i = values.find(key);
		if(i == values.end())
		{
			// Eintrag existiert noch nicht.
			values[key] = new ParameterContainer<T>(value);
		}
		else
		{
			// alten Eintrag löschen, dann überschreiben
			delete i->second;
			i->second = new ParameterContainer<T>(value);
		}
	}

	template<typename T> const T& get(const std::string& key) const
	{
		stdext::hash_map<std::string, GenericParameterContainer*>::const_iterator i = values.find(key);
		if(i == values.end())
		{
			// Eintrag existiert nicht!
			throw 0;
		}
		else
		{
			// versuchen, zu casten
			const GenericParameterContainer* p_container = i->second;
			const ParameterContainer<T>* p_spec = dynamic_cast<const ParameterContainer<T>*>(p_container);
			if(!p_spec)
			{
				// Falscher Typ!
				throw "Falscher Parametertyp!";
			}
			else
			{
				return p_spec->getValue();
			}
		}
	}

	bool has(const std::string& key) const
	{
		stdext::hash_map<std::string, GenericParameterContainer*>::const_iterator i = values.find(key);
		return i != values.end();
	}

	void clear()
	{
		// alle Parameter löschen
		for(stdext::hash_map<std::string, GenericParameterContainer*>::const_iterator i = values.begin(); i != values.end(); ++i)
		{
			delete i->second;
		}

		values.clear();
	}

private:
	class GenericParameterContainer
	{
	public:
		GenericParameterContainer()
		{
		}

		virtual ~GenericParameterContainer()
		{
		}

		virtual GenericParameterContainer* copy()
		{
			return 0;
		}
	};

	template<typename T> class ParameterContainer : public GenericParameterContainer
	{
	public:
		ParameterContainer(const T& value) : myValue(value)
		{
		}

		~ParameterContainer()
		{
		}

		GenericParameterContainer* copy()
		{
			return new ParameterContainer<T>(myValue);
		}

		const T& getValue() const
		{
			return myValue;
		}

	private:
		const T myValue;
	};

	stdext::hash_map<std::string, GenericParameterContainer*> values;
};

#endif