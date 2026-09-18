#include "pch.h"
#include "bomb.h"
#include "player.h"
#include "damage.h"
#include "tileset.h"
#include "engine.h"
#include "particlesystem.h"

Bomb::Bomb(Level& level,
		   const Vec2i& position) : Object(level, 1)
{
	renderLayers = RL_MAIN | RL_LIGHT;
	warpTo(position);
	flags = OF_MASSIVE | OF_COLLECTABLE | OF_TRANSPORTABLE;
	countDown = 0;
}

Bomb::~Bomb()
{
}

void Bomb::updateSprites()
{
	// bomb
	if(flags & OF_COLLECTABLE) sprites.add(Vec2i(0, 160));
	else sprites.add(Vec2i(32 + 32 * ((countDown / 6) % 4), 160));
}

void Bomb::onRender(RenderLayer layer,
					const Vec4f& color)
{
	if(layer == RL_MAIN) Engine::inst().renderSprites(sprites, color);
	else if(layer == RL_LIGHT)
	{
		if(!(flags & OF_COLLECTABLE) &&
		   deathCountDown != 1.0f)
		{
			level.renderShine(deathCountDown * 5.0f, deathCountDown * 3.0f);
		}
	}
}

void Bomb::onUpdate()
{
	if(!(flags & OF_COLLECTABLE))
	{
		if(!countDown) countDown = 150;
		else
		{
			// smoke
			float c = static_cast<float>(countDown) / 150.0f;
			ParticleSystem* p_particleSystem = level.getParticleSystem();
			ParticleSystem* p_fireParticleSystem = level.getFireParticleSystem();
			ParticleSystem::Particle p;
			p.lifetime = 100;
			p.damping = 0.99f;
			p.gravity = 0.005f;
			p.positionOnTexture = Vec2b(0, 0);
			p.sizeOnTexture = Vec2b(16, 16);
			p.position = position * 16 + Vec2i(11, 4);
			p.velocity = Vec2f(random(-0.5f, 0.5f), -1.0f);
			p.color = Vec4f(c, c, c, 0.2f);
			p.deltaColor = Vec4f(0.0f, 0.0f, 0.0f, -0.2f / p.lifetime);
			p.rotation = random(0.0f, 10.0f);
			p.deltaRotation = random(-0.1f, 0.1f);
			p.size = random(0.3f, 0.5f);
			p.deltaSize = random(0.01f, 0.05f);
			p_particleSystem->addParticle(p);

			countDown--;
			if(!countDown)
			{
				// Explosion!
				disappear(0.2f);
				level.addCameraShake(1.0f);
				level.addFlash(2.0f);
				Engine::inst().playSound("explosion.ogg", false, 0.15f, 100);

				// The bomb bursts apart too and needs its own block for it:
				// the loop below picks its victims with getFrontObjectAt,
				// which skips anything that has already called disappear() -
				// precisely the bomb. Without this block an explosion that
				// hits nothing would throw no debris at all.
				{
					const Sprites& debris = getSprites();
					const int numTries = debris.getTryCount(random(30, 40));
					for(int i = 0; i < numTries; i++)
					{
						p.lifetime = static_cast<ushort>(random(40, 80));
						p.damping = 0.95f;
						p.gravity = 0.075f;
						p.positionOnTexture = Vec2b(96, 0);
						p.sizeOnTexture = Vec2b(16, 16);

						Vec4f sampled;
						Vec2i offset;
						if(!debris.sample(&sampled, &offset)) continue;

						p.position = position * 16 + offset;
						const float r = random(0.0f, 6.283f);
						p.velocity = random(3.0f, 6.0f) * Vec2f(sin(r), cos(r));
						p.color = sampled + Vec4f(0.0f, 0.0f, 0.0f, random(0.3f, 0.5f));
						p.deltaColor = Vec4f(0.0f, 0.0f, 0.0f, -p.color.a / p.lifetime);
						p.rotation = random(0.0f, 10.0f);
						p.deltaRotation = random(-0.1f, 0.1f);
						p.size = random(0.5f, 1.0f);
						p.deltaSize = -p.size / p.lifetime;
						p_particleSystem->addParticle(p);
					}
				}

				int tileID = level.getTileAt(0, position);
				const TileSet::TileInfo& tileInfo = level.getTileSet()->getTileInfo(tileID);
				if(tileInfo.type != 3) new Damage(level, position);

				for(int x = -1; x <= 1; x++)
				{
					for(int y = -1; y <= 1; y++)
					{
						bool destroyed = false;
						const Sprites* p_sprites = 0;

						Vec2i pos = position + Vec2i(x, y);
						int tileID = level.getTileAt(1, pos);
						const TileSet::TileInfo& tileInfo = level.getTileSet()->getTileInfo(tileID);
						if(tileInfo.type == 2)
						{
							// destroy the tile
							level.setTileAt(1, pos, 0);
							destroyed = true;
							p_sprites = &tileInfo.sprites;
						}

						Object* p_obj = level.getFrontObjectAt(pos);
						if(p_obj)
						{
							p_obj->onExplosion();
							if(!p_obj->isAlive())
							{
								destroyed = true;
								p_sprites = &p_obj->getSprites();
							}
						}

						if(destroyed && p_sprites)
						{
							// debris
							int n = p_sprites->getTryCount(random(30, 40));
							for(int i = 0; i < n; i++)
							{
								p.lifetime = static_cast<ushort>(random(40, 80));
								p.damping = 0.95f;
								p.gravity = 0.075f;
								p.positionOnTexture = Vec2b(96, 0);
								p.sizeOnTexture = Vec2b(16, 16);

								Vec4f sampled;
								Vec2i offset;
								if(!p_sprites->sample(&sampled, &offset)) continue;

								p.position = pos * 16 + offset + Vec2i(random(-2, 2), random(-2, 2));
								p.velocity = random(4.0f, 7.0f) * Vec2f(x, y).normalize() + Vec2f(random(-0.2f, 0.2f), random(-0.2f, 0.2f));
								p.color = sampled + Vec4f(0.0f, 0.0f, 0.0f, random(0.3f, 0.5f));
								p.deltaColor = Vec4f(0.0f, 0.0f, 0.0f, -p.color.a / p.lifetime);
								p.rotation = random(0.0f, 10.0f);
								p.deltaRotation = random(-0.1f, 0.1f);
								p.size = random(0.5f, 1.0f);
								p.deltaSize = -p.size / p.lifetime;
								p_particleSystem->addParticle(p);
							}
						}
					}
				}

				// blast wave
				for(int i = 0; i < 500; i++)
				{
					p.lifetime = 100;
					p.damping = 0.99f;
					p.gravity = 0.0f;
					p.positionOnTexture = Vec2b(0, 0);
					p.sizeOnTexture = Vec2b(16, 16);
					p.position = position * 16 + Vec2i(8, 8);
					const float r = random(0.0f, 6.283f);
					p.velocity = random(13.0f, 15.0f) * Vec2f(sin(r), cos(r));
					p.color = Vec4f(1.0f, 1.0f, 1.0f, 0.2f);
					p.deltaColor = Vec4f(0.0f, 0.0f, 0.0f, -0.2f / p.lifetime);
					p.rotation = random(0.0f, 10.0f);
					p.deltaRotation = random(-0.1f, 0.1f);
					p.size = random(0.3f, 0.5f);
					p.deltaSize = random(0.01f, 0.05f);
					p_particleSystem->addParticle(p);
				}

				// fireball
				for(int i = 0; i < 500; i++)
				{
					p.lifetime = static_cast<ushort>(random(100, 200));
					p.damping = 0.8f;
					p.gravity = 0.0f;
					p.positionOnTexture = Vec2b(32, 0);
					p.sizeOnTexture = Vec2b(16, 16);
					p.position = position * 16 + Vec2i(8, 8);
					const float r = random(0.0f, 6.283f);
					p.velocity = random(1.0f, 8.0f) * Vec2f(sin(r), cos(r));
					p.color = Vec4f(random(0.5f, 1.0f), random(0.5f, 1.0f), 1.0f, random(0.05f, 0.15f));
					const float dc = -1.0f / (p.lifetime + random(-25, 25));
					p.deltaColor = Vec4f(dc, dc, dc, -p.color.a / p.lifetime);
					p.rotation = random(0.0f, 10.0f);
					p.deltaRotation = random(-0.025f, 0.025f);
					p.size = random(0.3f, 0.7f);
					p.deltaSize = random(0.01f, 0.02f);
					if(randomInt() % 3) p_particleSystem->addParticle(p);
					else p_fireParticleSystem->addParticle(p);
				}

				// core
				for(int i = 0; i < 100; i++)
				{
					p.lifetime = static_cast<ushort>(random(100, 150));
					p.damping = 0.7f;
					p.gravity = 0.0f;
					p.positionOnTexture = Vec2b(64, 0);
					p.sizeOnTexture = Vec2b(16, 16);
					p.position = position * 16 + Vec2i(8, 8);
					const float r = random(0.0f, 6.283f);
					p.velocity = random(0.0f, 5.0f) * Vec2f(sin(r), cos(r));
					p.color = Vec4f(random(0.75f, 1.0f), random(0.4f, 0.75f), random(0.0f, 0.25f), 0.2f);
					p.deltaColor = Vec4f(0.0f, 0.0f, 0.0f, -0.25f / p.lifetime);
					p.rotation = random(0.0f, 10.0f);
					p.deltaRotation = random(-0.05f, 0.05f);
					p.size = random(0.5f, 0.8f);
					p.deltaSize = random(0.01f, 0.02f);
					p_fireParticleSystem->addParticle(p);
				}
			}
		}
	}
}

void Bomb::onCollect(Player* p_player)
{
	// give the bomb to the player
	p_player->addInventory(0, 1);
	disappear(0.2f);

	Engine::inst().playSound("bomb.ogg", false, 0.15f, 100);
}

void Bomb::onExplosion()
{
	if(flags & OF_COLLECTABLE)
	{
		flags &= ~OF_COLLECTABLE;
		countDown = 5;
	}
	else if(countDown > 5) countDown = 5;
}

bool Bomb::reflectLaser(Vec2i& dir,
						bool lightBarrier)
{
	if(!lightBarrier) onExplosion();
	return false;
}

bool Bomb::reflectProjectile(Vec2f& velocity)
{
	onExplosion();
	return false;
}

void Bomb::onFire()
{
	onExplosion();
}