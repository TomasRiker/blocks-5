#include "pch.h"
#include "toxicwaste.h"
#include "toxicgas.h"
#include "engine.h"
#include "particlesystem.h"

ToxicWaste::ToxicWaste(Level& level,
					   const Vec2i& position) : Object(level, 1)
{
	warpTo(position);
	flags = OF_MASSIVE | OF_TRANSPORTABLE | OF_DESTROYABLE;
	destroyTime = 1;
	debrisColor = Vec4d(0.5, 0.6, 0.5, 0.25);
}

ToxicWaste::~ToxicWaste()
{
}

void ToxicWaste::onRender(int layer,
						  const Vec4d& color)
{
	if(layer == 1)
	{
		// Giftmüllfass rendern
		Engine::inst().renderSprite(Vec2i(0, 0), Vec2i(192, 352), Vec2i(16, 16), color);
	}
}

void ToxicWaste::onUpdate()
{
}

void ToxicWaste::onExplosion()
{
	disappear(0.2);

	ParticleSystem* p_particleSystem = level.getParticleSystem();
	ParticleSystem::Particle p;

	// grüne Giftgaswolke
	for(int i = 0; i < 250; i++)
	{
		p.lifetime = random(150, 300);
		p.damping = 0.96f;
		p.gravity = -0.005f;
		if(random() % 2) p.positionOnTexture = Vec2b(0, 64);
		else p.positionOnTexture = Vec2b(0, 0);
		p.sizeOnTexture = Vec2b(16, 16);
		p.position = position * 16 + Vec2i(random(6, 10), random(6, 10));
		const double r = random(0.0, 6.283);
		p.velocity = random(0.0, 3.0) * Vec2d(sin(r), cos(r));
		p.color = Vec4d(random(0.4, 1.0), random(0.75, 1.0), random(0.0, 0.5), random(0.15, 0.4));
		p.deltaColor = Vec4d(0.0, 0.0, 0.0, -p.color.a / p.lifetime);
		p.rotation = random(0.0f, 10.0f);
		p.deltaRotation = random(-0.05f, 0.05f);
		p.size = random(0.3f, 1.0f);
		p.deltaSize = random(0.0f, 0.025f);
		p_particleSystem->addParticle(p);
	}

	// Gasobjekt erzeugen
	new ToxicGas(level, position);
}

bool ToxicWaste::reflectLaser(Vec2i& dir,
							  bool lightBarrier)
{
	if(!lightBarrier) onExplosion();
	return false;
}

bool ToxicWaste::reflectProjectile(Vec2d& velocity)
{
	onExplosion();
	return false;
}

void ToxicWaste::onFire()
{
	onExplosion();
}