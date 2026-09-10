#include "pch.h"
#include "particlesystem.h"
#include "engine.h"
#include "texture.h"
#ifdef __EMSCRIPTEN__
#define _mm_prefetch(a, h) ((void)0)
#define _MM_HINT_T1 0
#else
#include <xmmintrin.h>
#endif

ParticleSystem::ParticleSystem(Texture* p_sprites)
{
	p_sprites->addRef();
	this->p_sprites = p_sprites;
}

ParticleSystem::~ParticleSystem()
{
	p_sprites->release();
}

uint ParticleSystem::peakCount = 0;

#ifdef PARTICLE_SYSTEM_USE_VERTEX_ARRAY
ParticleSystem::Vertex* ParticleSystem::vertexBuffer()
{
	// A function-local static rather than a member: it costs no allocation,
	// no destruction order and no lifetime question, and the array is plain
	// enough that zeroed storage is a valid starting state.
	static Vertex buffer[VERTEX_BUFFER_SIZE];
	return buffer;
}
#endif

uint ParticleSystem::takePeakCount()
{
	const uint peak = peakCount;
	peakCount = 0;
	return peak;
}

// #define PROFILE_PARTICLESYSTEM_RENDER
#define PREFETCH_RENDER

void ParticleSystem::render()
{
#ifdef PROFILE_PARTICLESYSTEM_RENDER
	BEGIN_PROFILE(renderParticleSystem)
#endif

	// TODO: parallelization: n threads each prepare a vertex buffer, which are then drawn one after another

	p_sprites->bind();

	const uint count = static_cast<uint>(particles.size());
	if(count > peakCount) peakCount = count;

#ifdef PARTICLE_SYSTEM_USE_VERTEX_ARRAY
	Vertex* const p_vertexBuffer = vertexBuffer();
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glVertexPointer(2, GL_FLOAT, sizeof(Vertex), &p_vertexBuffer->position);
	glTexCoordPointer(2, GL_FLOAT, sizeof(Vertex), &p_vertexBuffer->uv);
	glColorPointer(4, GL_FLOAT, sizeof(Vertex), &p_vertexBuffer->color);
	Vertex* p_vertex = p_vertexBuffer;
#else
	glBegin(GL_QUADS);
#endif

#ifdef PREFETCH_RENDER
	static const uint PREFETCH_STRIDE = 2;
	ParticleList::const_iterator prefetch = particles.begin();
	for(uint i = 0; i < PREFETCH_STRIDE && prefetch != particles.end(); ++i) ++prefetch;
#endif

	for(ParticleList::const_iterator i = particles.begin(); i != particles.end(); ++i)
	{
#ifdef PREFETCH_RENDER
		if(prefetch != particles.end())
		{
			const char* p_addr = reinterpret_cast<const char*>(&*prefetch);
			_mm_prefetch(p_addr, _MM_HINT_T1);
			_mm_prefetch(p_addr + 64, _MM_HINT_T1);
			++prefetch;
		}
#endif

		const Particle& p = *i;
		const float sinR = sinf(p.rotation);
		const float cosR = cosf(p.rotation);
		const Vec2f halfSize = static_cast<Vec2f>(p.sizeOnTexture) * 0.5f * p.size;

		// The two half-axes of the quad. They are perpendicular, but each keeps
		// its own length, so the second cannot be had by swapping the first
		// one's components - that would give a square whatever sprite went in.
		const Vec2f halfX(halfSize.x * cosR, -halfSize.x * sinR);
		const Vec2f halfY(halfSize.y * sinR, halfSize.y * cosR);

		// The quad is walked as an outline instead of being measured out from
		// the centre four times over: only the first corner is built from the
		// axes, and each of the others is one vector addition onto a corner
		// that is already there.
		const Vec2f edgeX = halfX + halfX;
		const Vec2f edgeY = halfY + halfY;
		const Vec2f corner0 = p.position - halfX - halfY;
		const Vec2f corner1 = corner0 + edgeX;
		const Vec2f corner2 = corner1 + edgeY;
		const Vec2f corner3 = corner0 + edgeY;

#ifdef PARTICLE_SYSTEM_USE_VERTEX_ARRAY
		if(p_vertex - p_vertexBuffer >= VERTEX_BUFFER_SIZE)
		{
			glDrawArrays(GL_QUADS, 0, p_vertex - p_vertexBuffer);
			p_vertex = p_vertexBuffer;
		}

		p_vertex[0].color = p_vertex[1].color = p_vertex[2].color = p_vertex[3].color = p.color;
		p_vertex[0].position = corner0;
		p_vertex[0].uv = p.positionOnTexture;
		p_vertex[1].position = corner1;
		p_vertex[1].uv = Vec2i(p.positionOnTexture.x + p.sizeOnTexture.x, p.positionOnTexture.y);
		p_vertex[2].position = corner2;
		p_vertex[2].uv = p.positionOnTexture + p.sizeOnTexture;
		p_vertex[3].position = corner3;
		p_vertex[3].uv = Vec2i(p.positionOnTexture.x, p.positionOnTexture.y + p.sizeOnTexture.y);
		p_vertex += 4;
#else
		glColor4fv(p.color);
		glTexCoord2i(p.positionOnTexture.x, p.positionOnTexture.y);
		glVertex2f(corner0.x, corner0.y);
		glTexCoord2i(p.positionOnTexture.x + p.sizeOnTexture.x, p.positionOnTexture.y);
		glVertex2f(corner1.x, corner1.y);
		glTexCoord2i(p.positionOnTexture.x + p.sizeOnTexture.x, p.positionOnTexture.y + p.sizeOnTexture.y);
		glVertex2f(corner2.x, corner2.y);
		glTexCoord2i(p.positionOnTexture.x, p.positionOnTexture.y + p.sizeOnTexture.y);
		glVertex2f(corner3.x, corner3.y);
#endif
	}

#ifdef PARTICLE_SYSTEM_USE_VERTEX_ARRAY
	if(p_vertex != p_vertexBuffer) glDrawArrays(GL_QUADS, 0, p_vertex - p_vertexBuffer);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
#else
	glEnd();
#endif

	p_sprites->unbind();

#ifdef PROFILE_PARTICLESYSTEM_RENDER
	END_PROFILE(renderParticleSystem)
#endif
}

// Off like every other PROFILE_ toggle in the tree. Switched on, every run
// writes a line to the log and to the console, which during play means
// continuously.
// #define PROFILE_PARTICLESYSTEM_UPDATE
#define PREFETCH_UPDATE

void ParticleSystem::update()
{
#ifdef PROFILE_PARTICLESYSTEM_UPDATE
	BEGIN_PROFILE(updateParticleSystem)
#endif

#ifdef PREFETCH_UPDATE
	static const uint PREFETCH_STRIDE = 2;
	ParticleList::const_iterator prefetch = particles.begin();
	for(uint i = 0; i < PREFETCH_STRIDE && prefetch != particles.end(); ++i) ++prefetch;
#endif

	for(ParticleList::iterator i = particles.begin(); i != particles.end();)
	{
#ifdef PREFETCH_UPDATE
		if(prefetch != particles.end())
		{
			const char* p_addr = reinterpret_cast<const char*>(&*prefetch);
			_mm_prefetch(p_addr, _MM_HINT_T1);
			_mm_prefetch(p_addr + 64, _MM_HINT_T1);
			++prefetch;
		}
#endif

		Particle& p = *i;
		p.size += p.deltaSize;
		--p.lifetime;

		// delete old and too small particles
		if(!p.lifetime || p.size <= 0.0f) i = particles.erase(i);
		else
		{
			p.position += p.velocity;
			p.velocity *= p.damping;
			p.velocity.y += p.gravity;
			p.color += p.deltaColor;
			p.rotation += p.deltaRotation;
			++i;
		}
	}

#ifdef PROFILE_PARTICLESYSTEM_UPDATE
	END_PROFILE(updateParticleSystem)
#endif
}

void ParticleSystem::addParticle(const Particle& particle)
{
	const double particleDensity = Engine::inst().getParticleDensity();
	if(particleDensity != 1.0 && mt.rand() > 0.25 + 0.75 * particleDensity) return;

	particles.push_back(particle);
}

ParticleSystem::Particle* ParticleSystem::getNewParticle()
{
	const double particleDensity = Engine::inst().getParticleDensity();
	if(particleDensity != 1.0 && mt.rand() > 0.25 + 0.75 * particleDensity) return 0;

	particles.push_back(Particle());
	return &particles.back();
}

void ParticleSystem::clear()
{
	particles.clear();
}
