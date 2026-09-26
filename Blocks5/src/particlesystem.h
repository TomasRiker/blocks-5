#ifndef _PARTICLESYSTEM_H
#define _PARTICLESYSTEM_H

/*** Class for a particle system ***/

// For Vertex, which the scratch buffer below is made of.
#include "renderer.h"

class Texture;

class ParticleSystem
{
public:
	// The numbers are byte offsets. The blank line separates what render()
	// reads from what only update() needs: the first six members are enough
	// for a vertex and fit into one cache line together. A colour is a Vec4f:
	// one 16-byte access and a single vector addition. sizeof(Particle) = 80.
	struct Particle
	{
		float rotation;				//  0
		float size;					//  4
		Vec4f color;				//  8
		Vec2b positionOnTexture;	// 24
		Vec2b sizeOnTexture;		// 26
		Vec2f position;				// 28

		// Who this particle belongs to and how many ticks it has left, sharing
		// four bytes; 65535 ticks is twenty-two minutes. An id of 0, the normal
		// case, means nobody: whatever wants to find its own particles again
		// stamps one in here and looks them up through begin()/end().
		ushort id;					// 36
		ushort lifetime;			// 38
		float deltaSize;			// 40
		Vec2f velocity;				// 44
		float damping;				// 52
		float gravity;				// 56
		Vec4f deltaColor;			// 60
		float deltaRotation;		// 76
									// 80

		// Everything to zero: the forty-nine callers of addParticle() declare
		// a Particle on the stack and set only what they need, so a member none
		// of them knows about would otherwise arrive as a random number. Vec's
		// own default constructor is empty, hence every member is named here.
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

	// The largest number of particles one system has had to draw since the
	// last call, and 0 again afterwards. It is what sizes the vertex buffer,
	// which every system shares, so which system it was does not matter. A
	// static and not an #ifdef, because a member that exists in some
	// translation units and not in others makes two different classes; the
	// cost is a comparison per system per frame.
	static uint takePeakCount();

	// The living particles, for direct access. Anything that wants only its
	// own checks id - filtering by it is an if and needs no method of its own.
	ParticleList::iterator begin() { return particles.begin(); }
	ParticleList::iterator end() { return particles.end(); }

private:
	Texture* p_sprites;
	ParticleList particles;
	MTRand mt;

	static uint peakCount;

	// Must be a multiple of 4, because each particle is a quad. 32768 is
	// 8192 particles and exactly one MiB, against a measured worst case of
	// 4506 in the whole campaign (level 41; the four levels of the last
	// chapter sit near 2900, and 24 of the 42 stay under 400). Above it
	// render() hands the particles to the renderer in several pieces, which
	// changes only the number of calls: nine bombs going off together
	// (LinuxBuild/test/particle_stress.xml) reach 8264.
	static const uint VERTEX_BUFFER_SIZE = 32768;

	// One buffer for every particle system, which is what makes the size
	// above affordable: it is scratch, filled and handed on within a single
	// render() call, and rendering is single-threaded. The level editor holds
	// six Levels of three systems each - the one being edited and the five
	// palettes - so a buffer per system would be eighteen megabytes, most of
	// it in palettes that never spawn a particle.
	static Vertex* vertexBuffer();
};

#endif