#include "pch.h"
#include "toxicgas.h"
#include "tileset.h"
#include "engine.h"
#include "sound.h"
#include "soundinstance.h"
#include "particlesystem.h"

uint ToxicGas::numInstances = 0;
SoundInstance* ToxicGas::p_soundInst = 0;

ToxicGas::ToxicGas(Level& level,
				   const Vec2i& position) : Object(level, 0)
{
	type = "ToxicGas";
	warpTo(position);
	flags = OF_PROXY | OF_NO_SHADOW;
	spreadCounter = random(50, 80);

	numInstances++;

	if(numInstances == 1)
	{
		// Das ist die erste Instanz. Sound abspielen!
		Sound* p_sound = Manager<Sound>::inst().request("gas.ogg");
		p_soundInst = p_sound->createInstance();
		p_sound->release();
		if(p_soundInst)
		{
			p_soundInst->setVolume(0.0);
			p_soundInst->play(true);
		}

		updateSound();
	}
}

ToxicGas::~ToxicGas()
{
}

void ToxicGas::onRemove()
{
	numInstances--;

	if(!numInstances)
	{
		if(p_soundInst)
		{
			// Das war die letzte Instanz. Sound stoppen.
			p_soundInst->stop();
			p_soundInst = 0;
		}
	}
	else updateSound();
}

void ToxicGas::onUpdate()
{
	if(!(random() % 3))
	{
		ParticleSystem* p_particleSystem = level.getParticleSystem();
		ParticleSystem* p_fireParticleSystem = level.getFireParticleSystem();
		ParticleSystem::Particle p;

		p.lifetime = random(10, 20);
		p.damping = 0.96f;
		p.gravity = -0.005f;
		if(random() % 2) p.positionOnTexture = Vec2b(0, 64);
		else p.positionOnTexture = Vec2b(0, 0);
		p.sizeOnTexture = Vec2b(16, 16);
		p.position = position * 16 + Vec2i(random(2, 14), random(2, 14));
		const double r = random(0.0, 6.283);
		p.velocity = random(0.0, 1.0) * Vec2d(sin(r), cos(r));
		p.color = Vec4d(random(0.4, 1.0), random(0.75, 1.0), random(0.0, 0.5), random(0.5, 1.5));
		p.deltaColor = Vec4d(0.0, 0.0, 0.0, -p.color.a / p.lifetime);
		p.rotation = random(0.0f, 10.0f);
		p.deltaRotation = random(-0.05f, 0.05f);
		p.size = 0.01f;
		p.deltaSize = random(0.05f, 0.25f);
		if(random() % 2) p_particleSystem->addParticle(p);
		else p_fireParticleSystem->addParticle(p);
	}

	std::vector<Object*> objects = level.getObjectsAt(position);
	for(std::vector<Object*>::const_iterator i = objects.begin(); i != objects.end(); ++i)
	{
		if((*i)->getFlags() & OF_BLOCK_GAS)
		{
			disappear(0.0);
			return;
		}
	}

	if(spreadCounter > 0) spreadCounter--;
	else if(spreadCounter == 0)
	{
		// in alle freien Richtungen ausbreiten
		for(int dir = 0; dir < 4; dir++)
		{
			Vec2i p = position + intToDir(dir);
			if(!level.isValidPosition(p)) continue;

			// wenn da schon Gas ist, abbrechen
			if(level.getAIFlags(p) & 2) continue;

			// Tiles blockieren das Gas.
			uint tileID = level.getTileAt(1, p);
			const TileSet::TileInfo& tileInfo = level.getTileSet()->getTileInfo(tileID);
			if(tileInfo.type == 1 || tileInfo.type == 2) continue;

			// Objekte?
			objects = level.getObjectsAt(p);
			bool blocked = false;
			for(std::vector<Object*>::const_iterator i = objects.begin(); i != objects.end(); ++i)
			{
				if((*i)->getFlags() & OF_BLOCK_GAS)
				{
					blocked = true;
					break;
				}
			}

			if(blocked) continue;

			// neues Gasobjekt erzeugen
			new ToxicGas(level, p);
		}

		spreadCounter = random(50, 80);
	}
}

void ToxicGas::frameBegin()
{
	level.setAIFlag(position, 2);
}

void ToxicGas::updateSound()
{
	if(!p_soundInst) return;

	double volume = 1.0 / (40.0 * 20.0) * numInstances;
	p_soundInst->setVolume(clamp(volume, 0.5, 1.0));
}