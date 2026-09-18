#ifndef _VEC_H
#define _VEC_H

/*** General vector class for vectors over arbitrary types with arbitrary dimension ***/

// Helper for taking the square root. One specialization per type, because
// an unqualified ::sqrt is the C one and takes a double: a float handed to it
// widens, goes through the double routine and narrows back, which is three
// times the work for a length and not one bit more accurate.
template<typename T> struct VecHelper
{
	static T sqrt(T x)
	{
		return ::sqrt(x);
	}
};

template<> struct VecHelper<float>
{
	static float sqrt(float x)
	{
		// Qualified, or it would find this function again.
		return ::sqrtf(x);
	}
};

template<> struct VecHelper<int>
{
	static int sqrt(int x)
	{
		return static_cast<int>(::sqrtf(static_cast<float>(x)));
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
// theirs with it. Each operation stands for the GL call it is named after and
// keeps that call's order of operations, which costs nothing and makes the
// two easy to read against each other; it is float throughout, where GL's own
// matrix stack was double in places (.claude/rules/rendering.md).
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
	static Mat4 scaling(float x, float y, float z)
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

	// gluPerspective's matrix. GLU builds it in double and this does not,
	// which reaches only the two entries that read zNear and zFar: m[10] and
	// m[14], the depth row. What puts a corner on the screen is m[0] and
	// m[5] - the cotangent of half the field of view, over the aspect - and
	// those read neither, so they come out bit for bit the same.
	static Mat4 perspective(float fovy, float aspect, float zNear, float zFar)
	{
		const float radians = fovy / 2.0f * 3.14159265358979323846f / 180.0f;
		const float sine = sinf(radians);
		const float cotangent = cosf(radians) / sine;
		const float deltaZ = zFar - zNear;
		Mat4 r = identity();
		r.m[0] = cotangent / aspect;
		r.m[5] = cotangent;
		r.m[10] = -(zFar + zNear) / deltaZ;
		r.m[11] = -1.0f;
		r.m[14] = -2.0f * zNear * zFar / deltaZ;
		r.m[15] = 0.0f;
		return r;
	}

	// gluLookAt on the identity: GLU's own float arithmetic for the three
	// axes, its glMultMatrixf, then its glTranslated of the eye. GLU takes
	// doubles and does the work in GLfloat, which is why these can be float
	// without moving an entry - a float difference formed in double and
	// rounded back is the float difference.
	static Mat4 lookAt(float eyeX, float eyeY, float eyeZ,
					   float centerX, float centerY, float centerZ,
					   float upX, float upY, float upZ)
	{
		float forward[3] = {centerX - eyeX, centerY - eyeY, centerZ - eyeZ};
		float up[3] = {upX, upY, upZ};
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
	void translate(float x, float y, float z)
	{
		m[12] = m[0] * x + m[4] * y + m[8] * z + m[12];
		m[13] = m[1] * x + m[5] * y + m[9] * z + m[13];
		m[14] = m[2] * x + m[6] * y + m[10] * z + m[14];
		m[15] = m[3] * x + m[7] * y + m[11] * z + m[15];
	}

	// glScaled: the three columns scaled in place.
	void scale(float x, float y, float z)
	{
		for(int i = 0; i < 4; i++)
		{
			m[i] *= x;
			m[4 + i] *= y;
			m[8 + i] *= z;
		}
	}

	// glRotated about one of the three axes, which is every rotation this
	// game asks for: Mesa's special case for each, multiplied on.
	void rotate(float degrees, float x, float y, float z)
	{
		float s, c;
		rotationTerms(degrees, &s, &c);
		Mat4 r = identity();
		if(x == 0.0f && y == 0.0f)
		{
			r.m[0] = c; r.m[5] = c;
			if(z < 0.0f) { r.m[4] = s; r.m[1] = -s; }
			else { r.m[4] = -s; r.m[1] = s; }
		}
		else if(x == 0.0f && z == 0.0f)
		{
			r.m[0] = c; r.m[10] = c;
			if(y < 0.0f) { r.m[8] = -s; r.m[2] = s; }
			else { r.m[8] = s; r.m[2] = -s; }
		}
		else
		{
			r.m[5] = c; r.m[10] = c;
			if(x < 0.0f) { r.m[9] = s; r.m[6] = -s; }
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

	// Mesa's sine and cosine for glRotated, in float: the radians round to
	// float before the trigonometry either way, which is why a right angle
	// has a cosine of -4.4e-8 and not 0. Forming them in double first and
	// rounding once buys a different last bit and nothing else - measured, it
	// moves a point at a 320-pixel radius by 0.000163 px, where the grid the
	// rasterizer snaps a vertex to is 1/256 of one.
	static void rotationTerms(float degrees, float* p_sin, float* p_cos)
	{
		const float radians = degrees * 3.14159265358979323846f / 180.0f;
		*p_sin = sinf(radians);
		*p_cos = cosf(radians);
	}

private:
	// GLU's helpers, in float.
	static void normalize3(float* v)
	{
		const float r = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
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