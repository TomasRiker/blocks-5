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
		// Float und nicht int: GL_INT ist in WebGL/GLES2 kein gueltiger Vertexattributtyp,
		// und es sind Texturpixelkoordinaten weit innerhalb des exakten Bereichs von
		// float. Dieselben 8 Byte, die Vertexanordnung bleibt also unveraendert.
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