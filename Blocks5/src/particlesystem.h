#ifndef _PARTICLESYSTEM_H
#define _PARTICLESYSTEM_H

#define PARTICLE_SYSTEM_USE_VERTEX_ARRAY

/*** Klasse für ein Partikelsystem ***/

class Texture;

class ParticleSystem
{
public:
	struct Particle
	{
		float rotation;				// 16
		float size;					// 20
		Vec4f color;				// 36
		Vec2b positionOnTexture;	// 38
		Vec2b sizeOnTexture;		// 40
		Vec2f position;				// 48

		float deltaSize;			// 52
		uint lifetime;				// 56
		Vec2f velocity;				// 64
		float damping;				// 68
		float gravity;				// 72
		Vec4f deltaColor;			// 88
		float deltaRotation;		// 92
	};

	ParticleSystem(Texture* p_sprites);
	~ParticleSystem();

	void render();
	void update();
	void addParticle(const Particle& particle);
	Particle* getNewParticle();
	void clear();

private:
#ifdef PARTICLE_SYSTEM_USE_VERTEX_ARRAY
	struct Vertex
	{
		Vec2f position;
		Vec2i uv;
		Vec4f color;
	};
#endif

	typedef std::list<Particle> ParticleList;

	Texture* p_sprites;
	ParticleList particles;
	MTRand mt;

#ifdef PARTICLE_SYSTEM_USE_VERTEX_ARRAY
	Vertex* p_vertexBuffer;
	static const uint VERTEX_BUFFER_SIZE = 1024;
#endif
};

#endif