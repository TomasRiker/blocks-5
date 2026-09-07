#ifndef _SINGLETON_H
#define _SINGLETON_H

/*** Singleton class ***/

template<typename T> class Singleton
{
protected:
	Singleton()
	{
	}

	Singleton(const Singleton<T>& rhs)
	{
	}

	~Singleton()
	{
	}

	Singleton<T>& operator = (const Singleton<T>& rhs)
	{
		return *this;
	}

public:
	static T& inst()
	{
		static T theOneAndOnly;
		return theOneAndOnly;
	}
};

#endif