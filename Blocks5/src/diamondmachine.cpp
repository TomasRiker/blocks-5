#include "pch.h"
#include "diamondmachine.h"
#include "presets.h"
#include "engine.h"
#include "particlesystem.h"
#include "soundinstance.h"

DiamondMachine::DiamondMachine(Level& level,
							   const Vec2i& position) : Object(level, 1)
{
	warpTo(position);
	flags = OF_MASSIVE | OF_FIXED | OF_BLOCK_GAS;
	p_objOnMe = 0;
	counter = -1;
	p_soundInst = 0;
}

DiamondMachine::~DiamondMachine()
{
}

void DiamondMachine::onRender(int layer,
							  const Vec4d& color)
{
	if(layer == 1)
	{
		// Maschine rendern
		Vec2i positionOnTexture(0, 128);
		if(level.isElectricityOn())
		{
			if(counter == -1) positionOnTexture.x = 32;
			else positionOnTexture.x = 64 + 32 * (min(counter, 80) / 20);
		}
		else positionOnTexture.x = 0;
		Engine::inst().renderSprite(Vec2i(0, 0), positionOnTexture, Vec2i(16, 16), color);
	}
}

void DiamondMachine::onUpdate()
{
	if(level.isElectricityOn())
	{
		// Befindet sich ein Objekt auf der Maschine?
		Object* p_obj = level.getFrontObjectAt(position - Vec2i(0, 1));
		if(p_obj)
		{
			if(!p_obj->isTeleporting() && (p_obj->getFlags() & OF_CONVERTABLE))
			{
				if(p_obj == p_objOnMe)
				{
					// Rauch
					ParticleSystem* p_particleSystem = level.getParticleSystem();
					ParticleSystem* p_fireParticleSystem = level.getFireParticleSystem();
					ParticleSystem::Particle p;
					p.lifetime = random(80, 120);
					p.damping = 0.99f;
					p.gravity = 0.005f;
					p.positionOnTexture = Vec2b(0, 0);
					p.sizeOnTexture = Vec2b(16, 16);
					p.position = position * 16 - Vec2i(0, 16) + Vec2i(random(0, 16), random(0, 16));
					p.velocity = Vec2d(random(-0.5, 0.5), -1.0);
					p.color = p_obj->getDebrisColor();
					p.deltaColor = Vec4d(0.0, 0.0, 0.0, -p.color.a / p.lifetime);
					p.rotation = random(0.0f, 10.0f);
					p.deltaRotation = random(-0.1f, 0.1f);
					p.size = random(0.3f, 0.5f);
					p.deltaSize = random(0.01f, 0.05f);
					if(random() % 2) p_particleSystem->addParticle(p);
					else p_fireParticleSystem->addParticle(p);

					counter++;
					if(!counter)
					{
						Engine::inst().playSound("diamondmachine.ogg", false, 0.0, 100);
					}
				}
				else
				{
					p_objOnMe = p_obj;
					counter = -1;
				}

				if(counter >= 100)
				{
					// Der Block wird umgewandelt.
					p_obj->disappearNextFrame(0.5);
					level.getPresets()->instancePreset("Diamond", position - Vec2i(0, 1), 0);
//					level.addNewObjects();
					counter = -1;
				}
			}
			else counter = -1;
		}
		else
		{
			p_objOnMe = 0;
			counter = -1;
		}
	}
	else counter = -1;

	if(counter == -1 && p_soundInst)
	{
		p_soundInst->stop();
		p_soundInst = 0;
	}
}