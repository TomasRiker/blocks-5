#ifndef _VEC_H
#define _VEC_H

/*** Allgemeine Vektorklasse fuer Vektoren ueber beliebigen Typen mit beliebiger Dimension ***/

// Helfer fuer das Wurzelziehen
template<typename T> struct VecHelper
{
	static T sqrt(T x)
	{
		return ::sqrt(x);
	}
};

template<> struct VecHelper<int>
{
	static int sqrt(int x)
	{
		return static_cast<int>(::sqrt(static_cast<float>(x)));
	}
};

template<typename T, int DIM> struct VecCoords
{
	T value[DIM];
};

template<typename T> struct VecCoords<T, 2>
{
	union
	{
		T value[2];
		struct { T x, y; };
		struct { T u, v; };
	};
};

template<typename T> struct VecCoords<T, 3>
{
	union
	{
		T value[3];
		struct { T x, y, z; };
		struct { T r, g, b; };
	};
};

template<typename T> struct VecCoords<T, 4>
{
	union
	{
		T value[4];
		struct { T x, y, z, w; };
		struct { T r, g, b, a; };
	};
};

template<typename T, int DIM> class Vec : public VecCoords<T, DIM>
{
public:
	Vec()
	{
	}

	Vec(T value)
	{
		for(int i = 0; i < DIM; i++)
			this->value[i] = value;
	}

	// Initialisierung mit 2 Parametern
	Vec(T x, T y)
	{
		this->value[0] = x;
		this->value[1] = y;
	}

	// Initialisierung mit 3 Parametern
	Vec(T x, T y, T z)
	{
		this->value[0] = x;
		this->value[1] = y;
		this->value[2] = z;
	}

	// Initialisierung mit 4 Parametern
	Vec(T x, T y, T z, T w)
	{
		this->value[0] = x;
		this->value[1] = y;
		this->value[2] = z;
		this->value[3] = w;
	}

	Vec(const Vec<T, DIM>& rhs)
	{
		for(int i = 0; i < DIM; i++)
			this->value[i] = rhs.value[i];
	}

	~Vec()
	{
	}

	T length() const
	{
		return VecHelper<T>::sqrt(lengthSq());
	}

	T lengthSq() const
	{
		T r = this->value[0] * this->value[0];
		for(int i = 1; i < DIM; i++)
			r += this->value[i] * this->value[i];
		return r;
	}

	Vec<T, DIM>& normalize()
	{
		return *this /= length();
	}

	Vec<T, DIM> normalizedCopy() const
	{
		return Vec<T, DIM>(*this).normalize();
	}

	bool isZero() const
	{
		T zero = static_cast<T>(0);
		for(int i = 0; i < DIM; i++)
			if(this->value[i] != zero) return false;
		return true;
	}

	const T& operator [] (unsigned int index) const
	{
		return this->value[index];
	}

	T& operator [] (unsigned int index)
	{
		return this->value[index];
	}

	Vec<T, DIM> operator + (const Vec<T, DIM>& rhs) const
	{
		Vec<T, DIM> r;
		for(int i = 0; i < DIM; i++)
			r.value[i] = this->value[i] + rhs.value[i];
		return r;
	}

	Vec<T, DIM> operator - (const Vec<T, DIM>& rhs) const
	{
		Vec<T, DIM> r;
		for(int i = 0; i < DIM; i++)
			r.value[i] = this->value[i] - rhs.value[i];
		return r;
	}

	Vec<T, DIM> operator - () const
	{
		Vec<T, DIM> r;
		for(int i = 0; i < DIM; i++)
			r.value[i] = -this->value[i];
		return r;
	}

	// komponentenweise Multiplikation
	Vec<T, DIM> operator * (const Vec<T, DIM>& rhs) const
	{
		Vec<T, DIM> r;
		for(int i = 0; i < DIM; i++)
			r.value[i] = this->value[i] * rhs.value[i];
		return r;
	}

	Vec<T, DIM> operator * (T rhs) const
	{
		Vec<T, DIM> r;
		for(int i = 0; i < DIM; i++)
			r.value[i] = this->value[i] * rhs;
		return r;
	}

	// komponentenweise Division
	Vec<T, DIM> operator / (const Vec<T, DIM>& rhs) const
	{
		Vec<T, DIM> r;
		for(int i = 0; i < DIM; i++)
			r.value[i] = this->value[i] / rhs.value[i];
		return r;
	}

	Vec<T, DIM> operator / (T rhs) const
	{
		Vec<T, DIM> r;
		for(int i = 0; i < DIM; i++)
			r.value[i] = this->value[i] / rhs;
		return r;
	}

	// Skalarprodukt
	T operator ^ (const Vec<T, DIM>& rhs) const
	{
		T r = this->value[0] * rhs.value[0];
		for(int i = 1; i < DIM; i++)
			r += this->value[i] * rhs.value[i];
		return r;
	}

	// Kreuzprodukt (nur sinnvoll fuer DIM = 3)
	Vec<T, 3> operator % (const Vec<T, 3>& rhs) const
	{
		return Vec<T, 3>(this->y * rhs.z - this->z * rhs.y,
						 this->z * rhs.x - this->x * rhs.z,
						 this->x * rhs.y - this->y * rhs.x);
	}

	Vec<T, DIM>& operator = (const Vec<T, DIM>& rhs)
	{
		for(int i = 0; i < DIM; i++)
			this->value[i] = rhs.value[i];
		return *this;
	}

	Vec<T, DIM>& operator += (const Vec<T, DIM>& rhs)
	{
		for(int i = 0; i < DIM; i++)
			this->value[i] += rhs.value[i];
		return *this;
	}

	Vec<T, DIM>& operator -= (const Vec<T, DIM>& rhs)
	{
		for(int i = 0; i < DIM; i++)
			this->value[i] -= rhs.value[i];
		return *this;
	}

	Vec<T, DIM>& operator *= (const Vec<T, DIM>& rhs)
	{
		for(int i = 0; i < DIM; i++)
			this->value[i] *= rhs.value[i];
		return *this;
	}

	Vec<T, DIM>& operator *= (T rhs)
	{
		for(int i = 0; i < DIM; i++)
			this->value[i] *= rhs;
		return *this;
	}

	Vec<T, DIM>& operator /= (const Vec<T, DIM>& rhs)
	{
		for(int i = 0; i < DIM; i++)
			this->value[i] /= rhs.value[i];
		return *this;
	}

	Vec<T, DIM>& operator /= (T rhs)
	{
		for(int i = 0; i < DIM; i++)
			this->value[i] /= rhs;
		return *this;
	}

	bool operator == (const Vec<T, DIM>& rhs) const
	{
		for(int i = 0; i < DIM; i++)
			if(this->value[i] != rhs.value[i]) return false;
		return true;
	}

	bool operator != (const Vec<T, DIM>& rhs) const
	{
		for(int i = 0; i < DIM; i++)
			if(this->value[i] != rhs.value[i]) return true;
		return false;
	}

	// Konvertierung in anderes Vektorformat (gleiche Dimension)
	template<typename U> operator Vec<U, DIM>() const
	{
		Vec<U, DIM> r;
		for(int i = 0; i < DIM; i++)
			r.value[i] = static_cast<U>(this->value[i]);
		return r;
	}

	operator const T* () const
	{
		return this->value;
	}

	operator T* ()
	{
		return this->value;
	}
};

// Multipliktion mit Skalar von links
template<typename T, int DIM> Vec<T, DIM> operator * (T lhs, const Vec<T, DIM>& rhs)
{
	return rhs * lhs;
}

#endif