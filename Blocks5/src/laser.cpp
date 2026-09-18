#include "pch.h"
#include "laser.h"
#include "particlesystem.h"
#include "soundinstance.h"
#include "tileset.h"
#include "engine.h"

uint Laser::numInstances = 0;
SoundInstance* Laser::p_soundInst = 0;
bool Laser::soundChanged = false;

Laser::Laser(Level& level,
			 const Vec2i& position,
			 int dir) : Object(level, 1)
{
	renderLayers = RL_MAIN | RL_EFFECT | RL_LIGHT | RL_SPARKLE;
	warpTo(position);
	flags = OF_MASSIVE | OF_FIXED | OF_DESTROYABLE | OF_TRANSPORTABLE;
	destroyTime = 125;
	this->dir = dir;
	counter = 0;
	on = 0.0f;

	if(!level.isInEditor())
	{
		numInstances++;

		if(numInstances == 1)
		{
			// This is the first instance. Create the sound instance and pause it.
			Sound* p_sound = Manager<Sound>::inst().request("laser.ogg");
			p_soundInst = p_sound->createInstance(true);
			p_sound->release();

			if(p_soundInst)
			{
				p_soundInst->setVolume(0.0f);
				p_soundInst->setPitch(0.1f);
				p_soundInst->play(true);
				p_soundInst->pause();
			}
		}
	}
}

Laser::~Laser()
{
}

void Laser::onRemove()
{
	if(!level.isInEditor())
	{
		numInstances--;
		if(!numInstances)
		{
			// The last instance is gone. Stop the sound.
			if(p_soundInst) p_soundInst->stop();
			p_soundInst = 0;
			soundChanged = false;
		}
	}
}

void Laser::updateSprites()
{
	// Laser
	sprites.add(Vec2i(level.isElectricityOn() ? 32 : 0, 192)).rotation = 90.0f * dir;
}

void Laser::onRender(RenderLayer layer,
					 const Vec4f& color)
{
	Vec2i sp = getShownPositionInPixels();

	if(layer == RL_MAIN) Engine::inst().renderSprites(sprites, color);
	else if(layer == RL_EFFECT || layer == RL_SPARKLE)
	{
		if(on > 0.0f && !beam.empty())
		{
			Vec2f oldDir(0.0f);
			Vec2f oldP(0.0f);
			Vec2f dir(0.0f);
			Vec2f p(0.0f);

			beamPoints.clear();

			std::list<Vec2f>::const_iterator last = beam.end();
			last--;
			uint n = 0;
			for(std::list<Vec2f>::const_iterator i = beam.begin(); i != beam.end(); ++i, ++n)
			{
				oldP = p;
				oldDir = dir;
				p = *i;
				dir = p - oldP;

				// The first two points and the last always: the second is
				// where the first real direction exists, and only from the
				// third on does a point that keeps it add nothing.
				if(n < 2 || i == last || dir != oldDir)
				{
					beamPoints.push_back(static_cast<Vec2f>(p));
				}
			}

			// render the inner and the outer beam
			Renderer& renderer = Renderer::inst();
			renderer.push();
			// Onto the cell's centre line, which is where the ruby is - see
			// BEAM_DRAW_OFFSET. Every width below is then the number of
			// pixels it lights: two for the core, the ruby's own two columns,
			// and six for the glow, two more to each side of it.
			renderer.translate(-sp.x + BEAM_DRAW_OFFSET, -sp.y + BEAM_DRAW_OFFSET);

			float x = static_cast<float>(counter) * 0.8f;
			Vec4f color;
			if(layer == RL_EFFECT) color = Vec4f(1.0f, 0.25f, 0.0f, on * deathCountDown * (0.2f + 0.05f * sin(x)));
			else color = Vec4f(0.0f, 0.25f, 0.0f, 0.4f * on * deathCountDown * (0.2f + 0.05f * sin(x)));
			renderer.polyline(beamPoints, 6.0f, static_cast<Vec4f>(color));
			renderer.point(static_cast<Vec2f>(p), 6.0f, static_cast<Vec4f>(color));

			const float green = 0.625f + 0.025f * glowJitter;
			if(layer == RL_EFFECT) color = Vec4f(1.0f, green, 0.0f, on * deathCountDown * (0.9f + 0.1f * cos(x)));
			else color = Vec4f(0.0f, green, 0.0f, 0.4f * on * deathCountDown * (0.9f + 0.1f * cos(x)));
			renderer.polyline(beamPoints, 2.0f, static_cast<Vec4f>(color));
			renderer.point(static_cast<Vec2f>(p), 4.0f, static_cast<Vec4f>(color));

			renderer.pop();
		}
	}
	else if(layer == RL_LIGHT)
	{
		// The jitter ramps with the beam, so a laser coming on brightens
		// evenly instead of flickering at full depth from the first tick.
		if(on > 0.0f) level.renderBeamShines(beam, sp, 0.25f, on * 0.4f, on * 0.05f, glowJitter);
	}
}

void Laser::onUpdate()
{
	soundChanged = false;

	// Once fire or a beam has worn destroyTime down below 5 the laser runs
	// the rest of the countdown itself and bursts on reaching 0 - once, on
	// the step that gets there, and the count stops.
	const bool burst = destroyTime > 0 && destroyTime < 5 && --destroyTime == 0;
	if(burst)
	{
		Engine::inst().playSound("vaporize.ogg", false, 0.15f);

		// debris
		ParticleSystem* p_particleSystem = level.getParticleSystem();
		ParticleSystem::Particle p;
		const Sprites& debris = getSprites();
		int n = debris.getTryCount(random(50, 80));
		for(int i = 0; i < n; i++)
		{
			p.lifetime = static_cast<ushort>(random(60, 120));
			p.damping = 0.9f;
			p.gravity = -0.1f;
			p.positionOnTexture = Vec2b(96, 0);
			p.sizeOnTexture = Vec2b(16, 16);

			Vec4f sampled;
			Vec2i offset;
			if(!debris.sample(&sampled, &offset)) continue;

			p.position = position * 16 + offset + Vec2i(random(-2, 2), random(-2, 2));
			p.velocity = Vec2f(random(-0.2f, 0.2f), random(-0.2f, 0.2f));
			p.color = sampled;
			p.deltaColor = Vec4f(0.0f, 0.0f, 0.0f, -p.color.a / p.lifetime);
			p.rotation = random(0.0f, 10.0f);
			p.deltaRotation = random(-0.1f, 0.1f);
			p.size = random(0.5f, 1.5f);
			p.deltaSize = random(0.01f, 0.05f);
			p_particleSystem->addParticle(p);
		}

		disappear(0.2f);
	}

	beam.clear();
	if(on > 0.0f)
	{
		// compute the beam
		Vec2i beamDir = numberToDir(dir);
		Vec2f beamPos = Vec2f(7.5f, 7.5f) + getShownPositionInPixels();
		Vec2i beamPosF;
		beam.push_back(beamPos);
		beamPos += beamDir;

		bool destroyed = false;
		bool infinity = false;
		const Sprites* p_sprites = 0;
		int z = 0;

		while(true)
		{
			beamPosF = beamPos / 16;

			beam.push_back(beamPos);
			if(!level.isValidPosition(beamPosF))
			{
				infinity = true;
				break;
			}

			bool reflected = false;

			// Is there something at this spot?
			Object* p_obj = 0;
			Vec2i tileHit;
			if(z > 2 && !level.isFreeAt2(beamPos, 0, &p_obj, &tileHit))
			{
				if(p_obj)
				{
					// An object blocks the way.
					if(beamDir.x) beamPos.x = 7.5f + p_obj->getShownPositionInPixels().x;
					else if(beamDir.y) beamPos.y = 7.5f + p_obj->getShownPositionInPixels().y;
					beam.back() = beamPos;

					if(p_obj->reflectLaser(beamDir))
					{
						// OK, the object deflected the laser!
						reflected = true;
					}
					else if(p_obj->getFlags() & OF_DESTROYABLE)
					{
						p_obj->setDestroyTime(p_obj->getDestroyTime() - 1);
						if(!p_obj->getDestroyTime())
						{
							p_obj->disappear(0.2f);
							destroyed = true;
							p_sprites = &p_obj->getSprites();
						}
					}
					else
					{
						beamPos -= 5 * beamDir;
						beam.back() = beamPos;
					}
				}
				else
				{
					// A tile blocks the way.
					int tileID = level.getTileAt(1, tileHit);
					const TileSet::TileInfo& tileInfo = level.getTileSet()->getTileInfo(tileID);
					if(tileInfo.type == 2)
					{
						level.setTileDestroyTimeAt(1, tileHit, level.getTileDestroyTimeAt(1, tileHit) - 1);
						if(!level.getTileDestroyTimeAt(1, tileHit))
						{
							level.setTileAt(1, tileHit, 0);
							destroyed = true;
							p_sprites = &tileInfo.sprites;
						}
					}

					if(beamDir.x) beamPos.x = 7.5f + tileHit.x * 16;
					else if(beamDir.y) beamPos.y = 7.5f + tileHit.y * 16;
					beam.back() = beamPos;

					if(tileInfo.type == 1)
					{
						beamPos -= 5 * beamDir;
						beam.back() = beamPos;
					}
				}

				if(reflected)
				{
					if(p_obj)
					{
						Object* p_newObjectHit;
						do
						{
							beamPos += beamDir;
							p_newObjectHit = 0;
							level.isFreeAt2(beamPos, 0, &p_newObjectHit, &tileHit);
						} while(p_newObjectHit == p_obj);
					}
				}
				else break;
			}

			beamPos += beamDir * 4;
			z++;
		}

		if(!infinity)
		{
			ParticleSystem* p_particleSystem = level.getParticleSystem();
			ParticleSystem* p_fireParticleSystem = level.getFireParticleSystem();
			ParticleSystem::Particle p;

			if(!(counter % 2))
			{
				// smoke
				p.lifetime = static_cast<ushort>(random(70, 100));
				p.damping = 0.99f;
				p.gravity = 0.005f;
				p.positionOnTexture = Vec2b(0, 0);
				p.sizeOnTexture = Vec2b(16, 16);
				p.position = beamPos + Vec2f(BEAM_DRAW_OFFSET, BEAM_DRAW_OFFSET);
				p.velocity = Vec2f(random(-0.25f, 0.25f), -1.0f);
				p.color = Vec4f(0.0f, 0.0f, 0.0f, on * 0.2f);
				p.deltaColor = Vec4f(0.0f, 0.0f, 0.0f, -p.color.a / p.lifetime);
				p.rotation = random(0.0f, 10.0f);
				p.deltaRotation = random(-0.1f, 0.1f);
				p.size = random(0.3f, 0.5f);
				p.deltaSize = random(0.01f, 0.02f);
				p_particleSystem->addParticle(p);
			}

			if(!(counter % 4))
			{
				// glowing particles
				p.lifetime = static_cast<ushort>(random(80, 120));
				p.damping = 0.9f;
				p.gravity = 0.1f;
				p.positionOnTexture = Vec2b(32, 32);
				p.sizeOnTexture = Vec2b(16, 16);
				// Two pixels of scatter about the point the beam ends on, and
				// nothing more: a particle's position is its centre, since
				// ParticleSystem::render builds the quad around it, and
				// beamPos is a point in the level rather than a cell's corner.
				// Where the same scatter is written "+ random(6, 10)" - the
				// fire, the toxic waste, an object arriving from a teleporter
				// - it is added to position * 16, and the eight in it is the
				// half cell that carries that corner to its centre.
				p.position = beamPos + Vec2f(BEAM_DRAW_OFFSET + random(-2.0f, 2.0f),
											 BEAM_DRAW_OFFSET + random(-2.0f, 2.0f));
				const float r = random(0.0f, 6.283f);
				p.velocity = random(3.0f, 6.0f) * Vec2f(sin(r), cos(r));
				p.color = Vec4f(random(0.5f, 1.0f), random(0.5f, 1.0f), 0.0f, on * 0.9f);
				p.deltaColor = Vec4f(0.5f, 0.0f, 0.0f, -p.color.a / p.lifetime);
				p.rotation = random(0.0f, 10.0f);
				p.deltaRotation = random(-0.1f, 0.1f);
				p.size = random(0.1f, 0.2f);
				p.deltaSize = random(-0.01f, -0.005f);
				if(randomInt() % 2) p_particleSystem->addParticle(p);
				else p_fireParticleSystem->addParticle(p);
			}

			if(destroyed && p_sprites)
			{
				Engine::inst().playSound("vaporize.ogg", false, 0.15f);

				// debris
				int n = p_sprites->getTryCount(random(50, 80));
				for(int i = 0; i < n; i++)
				{
					p.lifetime = static_cast<ushort>(random(60, 120));
					p.damping = 0.9f;
					p.gravity = -0.1f;
					p.positionOnTexture = Vec2b(96, 0);
					p.sizeOnTexture = Vec2b(16, 16);

					Vec4f sampled;
					Vec2i offset;
					if(!p_sprites->sample(&sampled, &offset)) continue;

					p.position = beamPosF * 16 + offset + Vec2i(random(-2, 2), random(-2, 2));
					p.velocity = Vec2f(random(-0.2f, 0.2f), random(-0.2f, 0.2f));
					p.color = sampled;
					p.deltaColor = Vec4f(0.0f, 0.0f, 0.0f, -p.color.a / p.lifetime);
					p.rotation = random(0.0f, 10.0f);
					p.deltaRotation = random(-0.1f, 0.1f);
					p.size = random(0.5f, 1.5f);
					p.deltaSize = random(0.01f, 0.05f);
					if(i % 2) p_particleSystem->addParticle(p);
					else p_fireParticleSystem->addParticle(p);
				}
			}
		}

		counter++;
	}

	if(level.isElectricityOn()) on += 0.2f;
	else on -= 0.2f;
	on = clamp(on, 0.0f, 1.0f);
}

void Laser::onElectricitySwitch(bool on)
{
	if(soundChanged) return;
	if(!p_soundInst) return;

	// control the sound
	if(on)
	{
		p_soundInst->resume();
		p_soundInst->slideVolume(0.25f, 0.2f);
		p_soundInst->slidePitch(1.0f, 0.2f);
	}
	else
	{
		p_soundInst->slideVolume(-1.0f, 0.1f);
		p_soundInst->slidePitch(0.1f, 0.1f);
	}

	soundChanged = true;
}

void Laser::frameBegin()
{
	Object::frameBegin();

	for(std::list<Vec2f>::const_iterator it = beam.begin();
		it != beam.end();
		++it)
	{
		// enemies should avoid lasers ...
		level.setAIFlag((*it) / 16, 1);
	}
}

bool Laser::changeInEditor(int mod)
{
	dir++;
	dir %= 4;

	return true;
}

void Laser::saveAttributes(TiXmlElement* p_target)
{
	p_target->SetAttribute("dir", dir);
}