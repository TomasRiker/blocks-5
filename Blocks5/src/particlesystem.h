#ifndef _PARTICLESYSTEM_H
#define _PARTICLESYSTEM_H

#define PARTICLE_SYSTEM_USE_VERTEX_ARRAY

/*** Class for a particle system ***/

class Texture;

class ParticleSystem
{
public:
	// The numbers are byte offsets. The blank line separates what render()
	// reads from what only update() needs: the first six members are enough
	// for a vertex and fit into one cache line together. Everything is single
	// precision (float), not double - a Vec4f is one 16-byte access and a
	// single vector addition, a Vec4d would be two. sizeof(Particle) = 80.
	struct Particle
	{
		float rotation;				//  0
		float size;					//  4
		Vec4f color;				//  8
		Vec2b positionOnTexture;	// 24
		Vec2b sizeOnTexture;		// 26
		Vec2f position;				// 28

		// Who this particle belongs to, and how many ticks it has left. The two
		// share the four bytes a single member would have taken, and neither is
		// anywhere near its ceiling - 65535 ticks is twenty-two minutes. 0 means
		// "nobody", which is the normal case for an id; anything that wants to
		// find its own particles again stamps one in here and looks them up
		// through begin()/end().
		ushort id;					// 36
		ushort lifetime;			// 38
		float deltaSize;			// 40
		Vec2f velocity;				// 44
		float damping;				// 52
		float gravity;				// 56
		Vec4f deltaColor;			// 60
		float deltaRotation;		// 76
									// 80

		// Everything to zero. The constructor is needed because the forty-nine
		// callers of addParticle() build a Particle on the stack and set only
		// what they need - a member none of them knows about would otherwise
		// arrive as a random number. Vec has an empty constructor of its own,
		// which is why a bare Particle() alone zeroes nothing.
		Particle()
			: rotation(0.0f), size(0.0f), color(0.0f),
			  positionOnTexture(0), sizeOnTexture(0), position(0.0f),
			  id(0), lifetime(0), deltaSize(0.0f), velocity(0.0f),
			  damping(0.0f), gravity(0.0f), deltaColor(0.0f), deltaRotation(0.0f)
		{
		}
	};

	typedef std::list<Particle> ParticleList;

	ParticleSystem(Texture* p_sprites);
	~ParticleSystem();

	void render();
	void update();
	void addParticle(const Particle& particle);
	Particle* getNewParticle();
	void clear();

	// The living particles, for direct access. Anything that wants only its
	// own checks id - filtering by it is an if and needs no method of its own.
	ParticleList::iterator begin() { return particles.begin(); }
	ParticleList::iterator end() { return particles.end(); }

private:
#ifdef PARTICLE_SYSTEM_USE_VERTEX_ARRAY
	struct Vertex
	{
		Vec2f position;
		// Float and not int: GL_INT is not a valid vertex attribute type in
		// WebGL/GLES2, and these are texture pixel coordinates well inside float's
		// exact range. The same 8 bytes, leaving the vertex layout unchanged.
		Vec2f uv;
		Vec4f color;
	};
#endif

	Texture* p_sprites;
	ParticleList particles;
	MTRand mt;

#ifdef PARTICLE_SYSTEM_USE_VERTEX_ARRAY
	Vertex* p_vertexBuffer;
	// Must be a multiple of 4, because each particle is a quad.
	static const uint VERTEX_BUFFER_SIZE = 4096;
#endif
};

#endif