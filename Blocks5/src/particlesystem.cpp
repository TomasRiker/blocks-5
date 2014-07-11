#include "pch.h"
#include "particlesystem.h"
#include "engine.h"
#include "texture.h"
#include <xmmintrin.h>

ParticleSystem::ParticleSystem(Texture* p_sprites)
{
	p_sprites->addRef();
	this->p_sprites = p_sprites;

#ifdef PARTICLE_SYSTEM_USE_VERTEX_ARRAY
	p_vertexBuffer = new Vertex[VERTEX_BUFFER_SIZE];
#endif
}

ParticleSystem::~ParticleSystem()
{
	p_sprites->release();

#ifdef PARTICLE_SYSTEM_USE_VERTEX_ARRAY
	delete[] p_vertexBuffer;
#endif
}

// #define PROFILE_PARTICLESYSTEM_RENDER
#define PREFETCH_RENDER

void ParticleSystem::render()
{
#ifdef PROFILE_PARTICLESYSTEM_RENDER
	BEGIN_PROFILE(renderParticleSystem)
#endif

	// TODO: Parallelisierung: n Threads bereiten jeweils einen Vertex-Buffer vor, die dann der Reihe nach gezeichnet werden

	p_sprites->bind();

#ifdef PARTICLE_SYSTEM_USE_VERTEX_ARRAY
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glVertexPointer(2, GL_FLOAT, sizeof(Vertex), &p_vertexBuffer->position);
	glTexCoordPointer(2, GL_INT, sizeof(Vertex), &p_vertexBuffer->uv);
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
		const Vec2f axisX(halfSize.x * cosR, -halfSize.y * sinR);

#ifdef PARTICLE_SYSTEM_USE_VERTEX_ARRAY
		if(p_vertex - p_vertexBuffer >= VERTEX_BUFFER_SIZE)
		{
			glDrawArrays(GL_QUADS, 0, p_vertex - p_vertexBuffer);
			p_vertex = p_vertexBuffer;
		}

		p_vertex[0].color = p_vertex[1].color = p_vertex[2].color = p_vertex[3].color = p.color;
		p_vertex[0].position = Vec2f(p.position.x - axisX.x + axisX.y, p.position.y - axisX.y - axisX.x);
		p_vertex[0].uv = p.positionOnTexture;
		p_vertex[1].position = Vec2f(p.position.x + axisX.x + axisX.y, p.position.y + axisX.y - axisX.x);
		p_vertex[1].uv = Vec2i(p.positionOnTexture.x + p.sizeOnTexture.x, p.positionOnTexture.y);
		p_vertex[2].position = Vec2f(p.position.x + axisX.x + -axisX.y, p.position.y + axisX.y + axisX.x);
		p_vertex[2].uv = p.positionOnTexture + p.sizeOnTexture;
		p_vertex[3].position = Vec2f(p.position.x - axisX.x + -axisX.y, p.position.y - axisX.y + axisX.x);
		p_vertex[3].uv = Vec2i(p.positionOnTexture.x, p.positionOnTexture.y + p.sizeOnTexture.y);
		p_vertex += 4;
#else
		glColor4fv(p.color);
		glTexCoord2i(p.positionOnTexture.x, p.positionOnTexture.y);
		glVertex2f(p.position.x - axisX.x + axisX.y, p.position.y - axisX.y - axisX.x);
		glTexCoord2i(p.positionOnTexture.x + p.sizeOnTexture.x, p.positionOnTexture.y);
		glVertex2f(p.position.x + axisX.x + axisX.y, p.position.y + axisX.y - axisX.x);
		glTexCoord2i(p.positionOnTexture.x + p.sizeOnTexture.x, p.positionOnTexture.y + p.sizeOnTexture.y);
		glVertex2f(p.position.x + axisX.x + -axisX.y, p.position.y + axisX.y + axisX.x);
		glTexCoord2i(p.positionOnTexture.x, p.positionOnTexture.y + p.sizeOnTexture.y);
		glVertex2f(p.position.x - axisX.x + -axisX.y, p.position.y - axisX.y + axisX.x);
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
	if(particles.size() > 1000) END_PROFILE(renderParticleSystem)
#endif
}

#define PROFILE_PARTICLESYSTEM_UPDATE
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

		// alte und zu kleine Partikel löschen
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
	if(particles.size() > 1000) END_PROFILE(updateParticleSystem)
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