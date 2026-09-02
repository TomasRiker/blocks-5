#ifndef _PARTICLESYSTEM_H
#define _PARTICLESYSTEM_H

#define PARTICLE_SYSTEM_USE_VERTEX_ARRAY

/*** Klasse fuer ein Partikelsystem ***/

class Texture;

class ParticleSystem
{
public:
	// Die Zahlen sind Byte-Offsets. Die Leerzeile trennt, was render() liest,
	// von dem, was nur update() braucht: die ersten sechs Felder reichen fuer
	// einen Vertex und passen zusammen in eine Cache-Zeile. Alles ist einfach
	// genau (float), nicht doppelt - ein Vec4f ist ein 16-Byte-Zugriff und eine
	// einzige Vektoraddition, ein Vec4d waeren zwei. sizeof(Particle) = 80.
	struct Particle
	{
		float rotation;				//  0
		float size;					//  4
		Vec4f color;				//  8
		Vec2b positionOnTexture;	// 24
		Vec2b sizeOnTexture;		// 26
		Vec2f position;				// 28

		float deltaSize;			// 36
		uint lifetime;				// 40
		Vec2f velocity;				// 44
		float damping;				// 52
		float gravity;				// 56
		Vec4f deltaColor;			// 60
		float deltaRotation;		// 76
	};								// 80

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
		// Float, not int: GL_INT is not a valid vertex-attribute type in WebGL /
		// GLES2, and these are texture pixel coordinates well inside float's exact
		// integer range. Same 8 bytes, so the vertex layout is unchanged.
		Vec2f uv;
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