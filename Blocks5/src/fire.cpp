#include "pch.h"
#include "fire.h"
#include "engine.h"
#include "particlesystem.h"

Fire::Fire(Level& level,
		   const Vec2i& position) : Object(level, 100)
{
	warpTo(position);
	flags = OF_FIXED;
	anim = position.x * 5 + position.y * 200;
}

Fire::~Fire()
{
}

void Fire::updateSprites()
{
	// Feuer
	sprites.add(Vec2i((anim / 5 % 8) * 32, 320), Vec4d(1.0, 1.0, 1.0, 0.5));
}

void Fire::onRender(int layer,
					const Vec4d& color)
{
	if(layer == 0) Engine::inst().renderSprites(sprites, color);
	else if(layer == 18)
	{
		level.renderShine(0.5, 1.0 + 0.05 * sin(anim / 5.0) + random(-0.05, 0.05));
	}
}

void Fire::onUpdate()
{
	// Feuer
	ParticleSystem* p_particleSystem = level.getParticleSystem();
	ParticleSystem* p_fireParticleSystem = level.getFireParticleSystem();
	ParticleSystem::Particle p;
	p.lifetime = random(60, 100);
	p.damping = 0.9f;
	p.gravity = -0.04f;
	p.positionOnTexture = Vec2b(32, 0);
	p.sizeOnTexture = Vec2b(16, 16);
	p.position = position * 16 + Vec2i(random(6, 10), random(6, 10));
	p.velocity = Vec2d(random(-0.5, 0.5), random(-0.5, 0.5));
	p.color = Vec4d(random(0.5, 1.0), random(0.8, 1.0), random(0.0, 0.25), random(0.2, 0.4));
	const double dc = -1.5 / (p.lifetime + random(-25, 25));
	p.deltaColor = Vec4d(dc, dc, dc, -p.color.a / p.lifetime);
	p.rotation = random(0.0f, 10.0f);
	p.deltaRotation = random(-0.1f, 0.1f);
	p.size = random(0.5f, 0.9f);
	p.deltaSize = random(-0.015f, -0.0075f);
	if(randomInt() % 2) p_particleSystem->addParticle(p);
	else p_fireParticleSystem->addParticle(p);

	// Befindet sich ein Objekt auf dem Feuer?
	const std::vector<Object*> objectsOnMe = level.getObjectsAt(position);
	for(std::vector<Object*>::const_iterator i = objectsOnMe.begin(); i != objectsOnMe.end(); ++i)
	{
		Object* p_obj = *i;
		if(p_obj == this) continue;

		p_obj->onFire();

		if(p_obj->getFlags() & OF_DESTROYABLE)
		{
			p_obj->setDestroyTime(p_obj->getDestroyTime() - 1);
			if(!p_obj->getDestroyTime())
			{
				p_obj->disappear(0.2);
				Object* p_destroyed = p_obj;

				Engine::inst().playSound("vaporize.ogg", false, 0.15);

				// Truemmer
				const Sprites& debris = p_destroyed->getSprites();
				int n = debris.getTryCount(random(50, 80));
				for(int i = 0; i < n; i++)
				{
					p.lifetime = random(60, 120);
					p.damping = 0.9f;
					p.gravity = -0.1f;
					p.positionOnTexture = Vec2b(96, 0);
					p.sizeOnTexture = Vec2b(16, 16);
					Vec4d sampled;
					Vec2i offset;
					if(!debris.sample(&sampled, &offset)) continue;

					p.position = p_obj->getPosition() * 16 + offset + Vec2i(random(-2, 2), random(-2, 2));
					p.velocity = Vec2d(random(-0.2, 0.2), random(-0.2, 0.2));
					p.color = sampled;
					p.deltaColor = Vec4d(0.0, 0.0, 0.0, -p.color.a / p.lifetime);
					p.rotation = random(0.0f, 10.0f);
					p.deltaRotation = random(-0.1f, 0.1f);
					p.size = random(0.5f, 1.5f);
					p.deltaSize = random(0.01f, 0.05f);
					if(randomInt() % 2) p_particleSystem->addParticle(p);
					else p_fireParticleSystem->addParticle(p);
				}

				if(p_obj->getFlags() & OF_KILL_FIRE)
				{
					// Das Feuer geht jetzt aus!
					const Sprites& ownDebris = getSprites();
					const int numTries = ownDebris.getTryCount(50);
					for(int i = 0; i < numTries; i++)
					{
						p.lifetime = random(80, 150);
						p.damping = 0.9f;
						p.gravity = -0.03f;
						p.positionOnTexture = Vec2b(0, 0);
						p.sizeOnTexture = Vec2b(16, 16);

						Vec4d sampled;
						Vec2i offset;
						if(!ownDebris.sample(&sampled, &offset)) continue;

						p.position = position * 16 + offset + Vec2i(random(-2, 2), random(-2, 2));
						p.velocity = Vec2d(random(-0.5, 0.5), random(-0.5, 0.5));
						p.color = sampled;
						const double dc = -0.5 / (p.lifetime + random(-25, 25));
						p.deltaColor = Vec4d(dc, dc, dc, -p.color.a / p.lifetime);
						p.rotation = random(0.0f, 10.0f);
						p.deltaRotation = random(-0.1f, 0.1f);
						p.size = random(0.6f, 0.9f);
						p.deltaSize = random(0.01f, 0.02f);
						if(randomInt() % 2) p_particleSystem->addParticle(p);
						else p_fireParticleSystem->addParticle(p);
					}

					disappear(0.2);
				}
			}
		}
	}

	anim++;
}