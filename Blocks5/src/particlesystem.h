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
	// einzige Vektoraddition, ein Vec4d waeren zwei. sizeof(Particle) = 84.
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

		// Wem dieses Teilchen gehoert. Ganz hinten, damit die ersten sechs
		// Felder ihre Cache-Zeile behalten. 0 heisst "niemandem", und das ist
		// der Normalfall; wer seine Teilchen spaeter wiederfinden will, traegt
		// hier eine Kennung ein und sucht sie ueber begin()/end().
		uint id;					// 80

		// Alles auf null. Der Konstruktor ist noetig, weil die neunundvierzig
		// Aufrufer von addParticle() sich ein Particle auf dem Stapel bauen und
		// nur setzen, was sie brauchen - jedes Feld, das eines von ihnen nicht
		// kennt, kaeme sonst als Zufallszahl an. Vec hat einen eigenen leeren
		// Konstruktor, ein Particle() allein nullt also nichts.
		Particle()
			: rotation(0.0f), size(0.0f), color(0.0f),
			  positionOnTexture(0), sizeOnTexture(0), position(0.0f),
			  deltaSize(0.0f), lifetime(0), velocity(0.0f), damping(0.0f),
			  gravity(0.0f), deltaColor(0.0f), deltaRotation(0.0f), id(0)
		{
		}
	};								// 84

	typedef std::list<Particle> ParticleList;

	ParticleSystem(Texture* p_sprites);
	~ParticleSystem();

	void render();
	void update();
	void addParticle(const Particle& particle);
	Particle* getNewParticle();
	void clear();

	// Die lebenden Teilchen, zum Anfassen. Wer nur seine eigenen will, prueft
	// id - danach zu filtern ist ein if und braucht keine eigene Methode.
	ParticleList::iterator begin() { return particles.begin(); }
	ParticleList::iterator end() { return particles.end(); }

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

	Texture* p_sprites;
	ParticleList particles;
	MTRand mt;

#ifdef PARTICLE_SYSTEM_USE_VERTEX_ARRAY
	Vertex* p_vertexBuffer;
	static const uint VERTEX_BUFFER_SIZE = 1024;
#endif
};

#endif