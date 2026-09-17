#ifndef _VEC_H
#define _VEC_H

/*** General vector class for vectors over arbitrary types with arbitrary dimension ***/

// Helper for taking the square root
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

	// initialisation with 2 parameters
	Vec(T x, T y)
	{
		this->value[0] = x;
		this->value[1] = y;
	}

	// initialisation with 3 parameters
	Vec(T x, T y, T z)
	{
		this->value[0] = x;
		this->value[1] = y;
		this->value[2] = z;
	}

	// initialisation with 4 parameters
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

	// component-wise multiplication
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

	// component-wise division
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

	// dot product
	T operator ^ (const Vec<T, DIM>& rhs) const
	{
		T r = this->value[0] * rhs.value[0];
		for(int i = 1; i < DIM; i++)
			r += this->value[i] * rhs.value[i];
		return r;
	}

	// cross product (only meaningful for DIM = 3)
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

	// conversion to another vector format (same dimension)
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

// multiplication by a scalar from the left
template<typename T, int DIM> Vec<T, DIM> operator * (T lhs, const Vec<T, DIM>& rhs)
{
	return rhs * lhs;
}

// A 4x4 float matrix in OpenGL's column-major order - the layout
// glUniformMatrix4fv takes as it is, which is what it exists for. The
// renderer's projection is one, and the 3D crossfades and the credits bake
// theirs with it: every operation here does Mesa's arithmetic for the GL
// call it stands for - float entries, its order of operations, its sine
// and cosine - so that a corner transformed by one lands where GL's own
// matrix stack put it (.claude/rules/rendering.md).
struct Mat4
{
	float m[16];

	static Mat4 identity()
	{
		Mat4 r;
		for(int i = 0; i < 16; i++) r.m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
		return r;
	}

	// The same matrix glOrtho builds, in float from the start.
	static Mat4 ortho(float left, float right, float bottom, float top, float zNear, float zFar)
	{
		Mat4 r = identity();
		r.m[0] = 2.0f / (right - left);
		r.m[5] = 2.0f / (top - bottom);
		r.m[10] = -2.0f / (zFar - zNear);
		r.m[12] = -(right + left) / (right - left);
		r.m[13] = -(top + bottom) / (top - bottom);
		r.m[14] = -(zFar + zNear) / (zFar - zNear);
		return r;
	}

	// glScaled on the identity, which stores the three factors as they are.
	static Mat4 scaling(double x, double y, double z)
	{
		Mat4 r = identity();
		r.scale(x, y, z);
		return r;
	}

	// The first two components of this * (x, y, 0, 1), summed as the
	// fixed-function vertex stage summed them: a texture coordinate under
	// a texture matrix.
	Vec<float, 2> transformPoint2D(const Vec<float, 2>& p) const
	{
		return Vec<float, 2>(m[0] * p.x + m[4] * p.y + m[8] * 0.0f + m[12],
							 m[1] * p.x + m[5] * p.y + m[9] * 0.0f + m[13]);
	}

	// gluPerspective's matrix: built in double as GLU builds it, then each
	// entry to float, which is what glMultMatrixd did with it.
	static Mat4 perspective(double fovy, double aspect, double zNear, double zFar)
	{
		const double radians = fovy / 2 * 3.14159265358979323846 / 180;
		const double sine = sin(radians);
		const double cotangent = cos(radians) / sine;
		const double deltaZ = zFar - zNear;
		Mat4 r = identity();
		r.m[0] = static_cast<float>(cotangent / aspect);
		r.m[5] = static_cast<float>(cotangent);
		r.m[10] = static_cast<float>(-(zFar + zNear) / deltaZ);
		r.m[11] = -1.0f;
		r.m[14] = static_cast<float>(-2 * zNear * zFar / deltaZ);
		r.m[15] = 0.0f;
		return r;
	}

	// gluLookAt on the identity: GLU's own float arithmetic for the three
	// axes, its glMultMatrixf, then its glTranslated of the eye.
	static Mat4 lookAt(double eyeX, double eyeY, double eyeZ,
					   double centerX, double centerY, double centerZ,
					   double upX, double upY, double upZ)
	{
		float forward[3] = {static_cast<float>(centerX - eyeX), static_cast<float>(centerY - eyeY), static_cast<float>(centerZ - eyeZ)};
		float up[3] = {static_cast<float>(upX), static_cast<float>(upY), static_cast<float>(upZ)};
		float side[3];
		normalize3(forward);
		cross3(forward, up, side);
		normalize3(side);
		cross3(side, forward, up);

		Mat4 axes = identity();
		axes.m[0] = side[0];    axes.m[4] = side[1];    axes.m[8] = side[2];
		axes.m[1] = up[0];      axes.m[5] = up[1];      axes.m[9] = up[2];
		axes.m[2] = -forward[0]; axes.m[6] = -forward[1]; axes.m[10] = -forward[2];
		Mat4 r = identity() * axes;
		r.translate(-eyeX, -eyeY, -eyeZ);
		return r;
	}

	// glTranslated: the products first, the old translation last, in float.
	void translate(double x, double y, double z)
	{
		const float fx = static_cast<float>(x), fy = static_cast<float>(y), fz = static_cast<float>(z);
		m[12] = m[0] * fx + m[4] * fy + m[8] * fz + m[12];
		m[13] = m[1] * fx + m[5] * fy + m[9] * fz + m[13];
		m[14] = m[2] * fx + m[6] * fy + m[10] * fz + m[14];
		m[15] = m[3] * fx + m[7] * fy + m[11] * fz + m[15];
	}

	// glScaled: the three columns scaled in place.
	void scale(double x, double y, double z)
	{
		const float fx = static_cast<float>(x), fy = static_cast<float>(y), fz = static_cast<float>(z);
		for(int i = 0; i < 4; i++)
		{
			m[i] *= fx;
			m[4 + i] *= fy;
			m[8 + i] *= fz;
		}
	}

	// glRotated about one of the three axes, which is every rotation this
	// game asks for: Mesa's special case for each, multiplied on.
	void rotate(double degrees, double x, double y, double z)
	{
		float s, c;
		rotationTerms(degrees, &s, &c);
		Mat4 r = identity();
		if(x == 0.0 && y == 0.0)
		{
			r.m[0] = c; r.m[5] = c;
			if(z < 0.0) { r.m[4] = s; r.m[1] = -s; }
			else { r.m[4] = -s; r.m[1] = s; }
		}
		else if(x == 0.0 && z == 0.0)
		{
			r.m[0] = c; r.m[10] = c;
			if(y < 0.0) { r.m[8] = -s; r.m[2] = s; }
			else { r.m[8] = s; r.m[2] = -s; }
		}
		else
		{
			r.m[5] = c; r.m[10] = c;
			if(x < 0.0) { r.m[9] = s; r.m[6] = -s; }
			else { r.m[9] = -s; r.m[6] = s; }
		}
		*this = *this * r;
	}

	// this * rhs, in GL's convention: the right-hand matrix is applied first.
	// Each entry is the four products summed left to right, as Mesa sums
	// them.
	Mat4 operator * (const Mat4& rhs) const
	{
		Mat4 r;
		for(int col = 0; col < 4; col++)
		{
			for(int row = 0; row < 4; row++)
			{
				r.m[col * 4 + row] = m[row] * rhs.m[col * 4] + m[4 + row] * rhs.m[col * 4 + 1]
								   + m[8 + row] * rhs.m[col * 4 + 2] + m[12 + row] * rhs.m[col * 4 + 3];
			}
		}
		return r;
	}

	// Mesa's sine and cosine for glRotated: the angle to float first, the
	// radians in double, sinf and cosf - which is why a right angle has a
	// cosine of -4.4e-8 and not 0.
	static void rotationTerms(double degrees, float* p_sin, float* p_cos)
	{
		const float angle = static_cast<float>(degrees);
		const double radians = angle * 3.14159265358979323846 / 180.0;
		*p_sin = sinf(static_cast<float>(radians));
		*p_cos = cosf(static_cast<float>(radians));
	}

private:
	// GLU's helpers: the length through double sqrt, the division in float.
	static void normalize3(float* v)
	{
		const float r = static_cast<float>(sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]));
		if(r == 0.0f) return;
		v[0] /= r;
		v[1] /= r;
		v[2] /= r;
	}

	static void cross3(const float* a, const float* b, float* out)
	{
		out[0] = a[1] * b[2] - a[2] * b[1];
		out[1] = a[2] * b[0] - a[0] * b[2];
		out[2] = a[0] * b[1] - a[1] * b[0];
	}
};

#endif