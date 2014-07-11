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
	warpTo(position);
	flags = OF_MASSIVE | OF_COLLECTABLE | OF_TRANSPORTABLE;
	countDown = 0;
}

Bomb::~Bomb()
{
}

void Bomb::onRender(int layer,
					const Vec4d& color)
{
	if(layer == 1)
	{
		// Bombe rendern
		if(flags & OF_COLLECTABLE)
		{
			Engine::inst().renderSprite(Vec2i(0, 0), Vec2i(0, 160), Vec2i(16, 16), color);
		}
		else
		{
			int frame = (countDown / 6) % 4;
			Engine::inst().renderSprite(Vec2i(0, 0), Vec2i(32 + 32 * frame, 160), Vec2i(16, 16), color);
		}
	}
	else if(layer == 18)
	{
		if(!(flags & OF_COLLECTABLE) &&
		   deathCountDown != 1.0)
		{
			level.renderShine(deathCountDown * 5.0, deathCountDown * 3.0);
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
			// Rauch
			double c = static_cast<double>(countDown) / 150.0;
			ParticleSystem* p_particleSystem = level.getParticleSystem();
			ParticleSystem* p_fireParticleSystem = level.getFireParticleSystem();
			ParticleSystem::Particle p;
			p.lifetime = 100;
			p.damping = 0.99f;
			p.gravity = 0.005f;
			p.positionOnTexture = Vec2b(0, 0);
			p.sizeOnTexture = Vec2b(16, 16);
			p.position = position * 16 + Vec2i(11, 4);
			p.velocity = Vec2d(random(-0.5, 0.5), -1.0);
			p.color = Vec4d(c, c, c, 0.2);
			p.deltaColor = Vec4d(0.0, 0.0, 0.0, -0.2 / p.lifetime);
			p.rotation = random(0.0f, 10.0f);
			p.deltaRotation = random(-0.1f, 0.1f);
			p.size = random(0.3f, 0.5f);
			p.deltaSize = random(0.01f, 0.05f);
			p_particleSystem->addParticle(p);

			countDown--;
			if(!countDown)
			{
				// Explosion!
				disappear(0.2);
				level.addCameraShake(1.0);
				level.addFlash(2.0);
				Engine::inst().playSound("explosion.ogg", false, 0.15, 100);
				int tileID = level.getTileAt(0, position);
				const TileSet::TileInfo& tileInfo = level.getTileSet()->getTileInfo(tileID);
				if(tileInfo.type != 3) new Damage(level, position);

				for(int x = -1; x <= 1; x++)
				{
					for(int y = -1; y <= 1; y++)
					{
						bool destroyed = false;
						Vec4d debrisColor;

						Vec2i pos = position + Vec2i(x, y);
						int tileID = level.getTileAt(1, pos);
						const TileSet::TileInfo& tileInfo = level.getTileSet()->getTileInfo(tileID);
						if(tileInfo.type == 2)
						{
							// Tile zerstören
							level.setTileAt(1, pos, 0);
							destroyed = true;
							debrisColor = tileInfo.debrisColor;
						}

						Object* p_obj = level.getFrontObjectAt(pos);
						if(p_obj)
						{
							p_obj->onExplosion();
							if(!p_obj->isAlive())
							{
								destroyed = true;
								debrisColor = p_obj->getDebrisColor();
							}
						}

						if(destroyed)
						{
							// Trümmer
							int n = random(30, 40);
							for(int i = 0; i < n; i++)
							{
								p.lifetime = random(40, 80);
								p.damping = 0.95f;
								p.gravity = 0.075f;
								p.positionOnTexture = Vec2b(96, 0);
								p.sizeOnTexture = Vec2b(16, 16);
								p.position = pos * 16 + Vec2i(random(-4, 20), random(-4, 20));
								p.velocity = random(4.0, 7.0) * Vec2d(x, y).normalize() + Vec2d(random(-0.2, 0.2), random(-0.2, 0.2));
								p.color = debrisColor + Vec4d(random(-0.1, 0.1), random(-0.1, 0.1), random(-0.1, 0.1), random(0.3, 0.5));
								p.deltaColor = Vec4d(0.0, 0.0, 0.0, -0.5 * -p.color.a / p.lifetime);
								p.rotation = random(0.0f, 10.0f);
								p.deltaRotation = random(-0.1f, 0.1f);
								p.size = random(0.5f, 1.0f);
								p.deltaSize = -p.size / p.lifetime;
								p_particleSystem->addParticle(p);
							}
						}
					}
				}

				// Druckwelle
				for(int i = 0; i < 500; i++)
				{
					p.lifetime = 100;
					p.damping = 0.99f;
					p.gravity = 0.0f;
					p.positionOnTexture = Vec2b(0, 0);
					p.sizeOnTexture = Vec2b(16, 16);
					p.position = position * 16 + Vec2i(8, 8);
					const double r = random(0.0, 6.283);
					p.velocity = random(13.0, 15.0) * Vec2d(sin(r), cos(r));
					p.color = Vec4d(1.0, 1.0, 1.0, 0.2);
					p.deltaColor = Vec4d(0.0, 0.0, 0.0, -0.2 / p.lifetime);
					p.rotation = random(0.0f, 10.0f);
					p.deltaRotation = random(-0.1f, 0.1f);
					p.size = random(0.3f, 0.5f);
					p.deltaSize = random(0.01f, 0.05f);
					p_particleSystem->addParticle(p);
				}

				// Feuerball
				for(int i = 0; i < 500; i++)
				{
					p.lifetime = random(100, 200);
					p.damping = 0.8f;
					p.gravity = 0.0f;
					p.positionOnTexture = Vec2b(32, 0);
					p.sizeOnTexture = Vec2b(16, 16);
					p.position = position * 16 + Vec2i(8, 8);
					const double r = random(0.0, 6.283);
					p.velocity = random(1.0, 8.0) * Vec2d(sin(r), cos(r));
					p.color = Vec4d(random(0.5, 1.0), random(0.5, 1.0), 1.0, random(0.05, 0.15));
					const double dc = -1.0 / (p.lifetime + random(-25, 25));
					p.deltaColor = Vec4d(dc, dc, dc, -p.color.a / p.lifetime);
					p.rotation = random(0.0f, 10.0f);
					p.deltaRotation = random(-0.025f, 0.025f);
					p.size = random(0.3f, 0.7f);
					p.deltaSize = random(0.01f, 0.02f);
					if(random() % 3) p_particleSystem->addParticle(p);
					else p_fireParticleSystem->addParticle(p);
				}

				// Kern
				for(int i = 0; i < 100; i++)
				{
					p.lifetime = random(100, 150);
					p.damping = 0.7f;
					p.gravity = 0.0f;
					p.positionOnTexture = Vec2b(64, 0);
					p.sizeOnTexture = Vec2b(16, 16);
					p.position = position * 16 + Vec2i(8, 8);
					const double r = random(0.0, 6.283);
					p.velocity = random(0.0, 5.0) * Vec2d(sin(r), cos(r));
					p.color = Vec4d(random(0.75, 1.0), random(0.4, 0.75), random(0.0, 0.25), 0.2);
					p.deltaColor = Vec4d(0.0, 0.0, 0.0, -0.25 / p.lifetime);
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
	// dem Spieler die Bombe geben
	p_player->addInventory(0, 1);
	disappear(0.2);

	Engine::inst().playSound("bomb.ogg", false, 0.15, 100);
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

bool Bomb::reflectProjectile(Vec2d& velocity)
{
	onExplosion();
	return false;
}

void Bomb::onFire()
{
	onExplosion();
}