#include "pch.h"
#include "projectile.h"
#include "engine.h"
#include "tileset.h"
#include "particlesystem.h"

const float Projectile::CANNON_SPEED = 1200.0f;

Projectile::Projectile(Level& level,
					   const Vec2f& positionInPixels,
					   const Vec2f& velocity) : Object(level, 0)
{
	renderLayers = RL_EFFECT | RL_LIGHT;
	type = "Projectile";
	warpTo(Vec2i(0, 0));
	flags = OF_PROXY | OF_NO_SHADOW;
	this->positionInPixels = positionInPixels;
	this->velocity = velocity;
	speed = velocity.length();
	// A shot placed from a level file starts still, and a zero vector
	// normalised is a NaN.
	if(speed > 0.0f) this->velocity /= speed;
	distance = 0.0f;
	life = 1.0f;
	reflectionCounter = 3;
}

Projectile::~Projectile()
{
}

void Projectile::onRender(RenderLayer layer,
						  const Vec4f& color)
{
	if(layer == RL_EFFECT && life > 0.0f)
	{
		float traceLength = min(distance, 0.035f * speed);

		Renderer& renderer = Renderer::inst();
		const Vec2f tail(positionInPixels - traceLength * velocity);
		const Vec2f head(positionInPixels);

		// render the glow
		renderer.setBlend(BM_ADDITIVE);
		renderer.line(tail, head, 5.0f, Vec4f(1.0f, 1.0f, 1.0f, 0.1f * min(life, 1.0f)));
		renderer.setBlend(BM_NORMAL);

		// render the projectile
		const Vec4f color(1.0f, 0.75f, 0.1f, life);
		renderer.line(tail, head, 2.0f, color);
		renderer.point(head, 2.0f, color);
	}
	else if(layer == RL_LIGHT)
	{
		// make the projectile shine
		const float s = fabsf(life);
		level.renderShine(0.5f * s, 0.4f * s, positionInPixels - Vec2f(8.0f, 8.0f));
	}
}

void Projectile::onUpdate()
{
	if(life == 1.0f)
	{
		float lengthToProcess = 0.02f * speed;
		while(lengthToProcess > 0.0f)
		{
			float step = min(lengthToProcess, 4.0f);
			lengthToProcess -= step;

			positionInPixels += step * velocity;
			distance += step;

			if(!level.isValidPosition(positionInPixels / 16))
			{
				life = 0.999f;
				break;
			}
			else
			{
				Object* p_objectHit = 0;
				Vec2i tileHit;
				bool bounce = false;
				bool destroyed = false;
				bool reflected = false;
				const Sprites* p_sprites = 0;
				Vec2f hitPosition;

				if(distance >= 8.0f && !level.isFreeAt2(positionInPixels, 0, &p_objectHit, &tileHit, 64.0f))
				{
					if(p_objectHit)
					{
						hitPosition = Vec2f(7.5f, 7.5f) + p_objectHit->getShownPositionInPixels();

						if(p_objectHit->reflectProjectile(velocity))
						{
							reflected = true;
							reflectionCounter--;
							distance = 0.0f;
							Engine::inst().playSound("ricochet.ogg", false, 0.2f);

							if(reflectionCounter < 0)
							{
								// This projectile has been reflected too often already!
								Vec2f perp(-velocity.y, velocity.x);
								velocity += random(-0.25f, 0.25f) * perp;
								life = 0.999f;
							}
						}

						if(!reflected)
						{
							if(p_objectHit->getFlags() & OF_DESTROYABLE)
							{
								p_objectHit->disappear(0.075f);
								destroyed = true;
								p_sprites = &p_objectHit->getSprites();
							}
							else
							{
								bounce = true;
							}
						}
					}
					else
					{
						hitPosition = Vec2f(7.5f, 7.5f) + tileHit * 16;

						// Is the tile destroyable?
						int tileID = level.getTileAt(1, tileHit);
						const TileSet::TileInfo& tileInfo = level.getTileSet()->getTileInfo(tileID);
						if(tileInfo.type == 2)
						{
							level.setTileAt(1, tileHit, 0);
							destroyed = true;
							p_sprites = &tileInfo.sprites;
						}
						else
						{
							bounce = true;
						}
					}

					ParticleSystem* p_particleSystem = level.getParticleSystem();
					ParticleSystem::Particle p;

					// dust
					for(int i = 0; i < 30; i++)
					{
						p.lifetime = static_cast<ushort>(random(25, 50));
						p.damping = 0.99f;
						p.gravity = -0.005f;
						p.positionOnTexture = Vec2b(0, 0);
						p.sizeOnTexture = Vec2b(16, 16);
						p.position = positionInPixels;
						const float r = random(0.0f, 6.283f);
						p.velocity = random(0.25f, 1.0f) * Vec2f(sinf(r), cosf(r));
						float c = random(0.75f, 1.0f);
						p.color = Vec4f(c, c, c, random(0.15f, 0.2f));
						p.deltaColor = -p.color / static_cast<float>(p.lifetime);
						p.rotation = random(0.0f, 10.0f);
						p.deltaRotation = random(-0.1f, 0.1f);
						p.size = random(0.2f, 0.3f);
						p.deltaSize = random(0.01f, 0.05f);
						p_particleSystem->addParticle(p);
					}

					// glowing particles
					for(int i = 0; i < 20; i++)
					{
						p.lifetime = static_cast<ushort>(random(80, 120));
						p.damping = 0.9f;
						p.gravity = 0.1f;
						p.positionOnTexture = Vec2b(32, 32);
						p.sizeOnTexture = Vec2b(16, 16);
						p.position = positionInPixels;
						const float r = random(0.0f, 6.283f);
						p.velocity = random(3.0f, 6.0f) * Vec2f(sinf(r), cosf(r));
						p.color = Vec4f(random(0.5f, 1.0f), random(0.5f, 1.0f), 0.0f, 0.9f);
						p.deltaColor = Vec4f(0.5f, 0.0f, 0.0f, -p.color.a / p.lifetime);
						p.rotation = random(0.0f, 10.0f);
						p.deltaRotation = random(-0.1f, 0.1f);
						p.size = random(0.1f, 0.2f);
						p.deltaSize = random(-0.01f, -0.005f);
						p_particleSystem->addParticle(p);
					}

					if(destroyed && p_sprites)
					{
						// The projectile has destroyed an object or a tile.

						// debris
						int n = p_sprites->getTryCount(random(30, 40));
						for(int i = 0; i < n; i++)
						{
							p.lifetime = static_cast<ushort>(random(40, 70));
							p.damping = 0.9f;
							p.gravity = 0.1f;
							p.positionOnTexture = Vec2b(96, 0);
							p.sizeOnTexture = Vec2b(16, 16);

							Vec4f sampled;
							Vec2i offset;
							if(!p_sprites->sample(&sampled, &offset)) continue;

							p.position = hitPosition + Vec2f(offset) - Vec2f(8.0f, 8.0f) + Vec2i(random(-2, 2), random(-2, 2));
							const float r = random(0.0f, 6.283f);
							p.velocity = random(2.0f, 5.0f) * Vec2f(sinf(r), cosf(r));
							p.color = sampled + Vec4f(0.0f, 0.0f, 0.0f, random(0.3f, 0.5f));
							p.deltaColor = Vec4f(0.0f, 0.0f, 0.0f, -p.color.a / p.lifetime);
							p.rotation = random(0.0f, 10.0f);
							p.deltaRotation = random(-0.1f, 0.1f);
							p.size = random(0.5f, 1.0f);
							p.deltaSize = -p.size / p.lifetime;
							p_particleSystem->addParticle(p);
						}

						Engine::inst().playSound("destroy.ogg", false, 0.1f);

						velocity = Vec2f(0.0f, 0.0f);
						life = -2.0f;
					}

					if(bounce)
					{
						// The projectile is to bounce off.
						Vec2f perp(-velocity.y, velocity.x);
						velocity *= -0.5f;
						velocity += random(-0.25f, 0.25f) * perp;

						Engine::inst().playSound("ricochet.ogg", false, 0.2f);

						life = 0.999f;
					}

					if(reflected)
					{
						// reduce the speed
						speed *= 0.8f;

						// correct the position
						positionInPixels = hitPosition;

						// move the projectile on a little, otherwise it would
						// hit the same object again
						distance = 0.0f;
						Object* p_newObjectHit;
						do
						{
							positionInPixels += velocity;
							distance += 1.0f;

							p_newObjectHit = 0;
							level.isFreeAt2(positionInPixels, 0, &p_newObjectHit, &tileHit, 64.0f);
						} while(p_newObjectHit == p_objectHit);
					}

					if(destroyed || bounce || reflected) break;
				}
			}
		}
	}
	else
	{
		positionInPixels += 0.02f * speed * velocity;
		distance += 0.02f * speed;

		// The projectile slowly disappears.
		if(life > 0.0f)
		{
			life -= 0.02f * 5.0f;
			if(life <= 0.0f) disappear(0.0f);
		}
		else if(life < 0.0f)
		{
			life += 0.02f * 5.0f;
			if(life >= 0.0f) disappear(0.0f);
		}
	}
}

void Projectile::saveExtendedAttributes(TiXmlElement* p_target)
{
	Object::saveExtendedAttributes(p_target);

	char s[256] = "";
	sprintf(s, "%f", positionInPixels.x); p_target->SetAttribute("positionInPixelsX", s);
	sprintf(s, "%f", positionInPixels.y); p_target->SetAttribute("positionInPixelsY", s);
	sprintf(s, "%f", velocity.x); p_target->SetAttribute("velocityX", s);
	sprintf(s, "%f", velocity.y); p_target->SetAttribute("velocityY", s);
	sprintf(s, "%f", speed); p_target->SetAttribute("speed", s);
	sprintf(s, "%f", distance); p_target->SetAttribute("distance", s);
	sprintf(s, "%f", life); p_target->SetAttribute("life", s);
	p_target->SetAttribute("reflectionCounter", reflectionCounter);
}

void Projectile::loadExtendedAttributes(TiXmlElement* p_element)
{
	Object::loadExtendedAttributes(p_element);

	const Vec2f cell = positionInPixels;
	p_element->QueryFloatAttribute("positionInPixelsX", &positionInPixels.x);
	p_element->QueryFloatAttribute("positionInPixelsY", &positionInPixels.y);
	p_element->QueryFloatAttribute("velocityX", &velocity.x);
	p_element->QueryFloatAttribute("velocityY", &velocity.y);
	p_element->QueryFloatAttribute("speed", &speed);
	p_element->QueryFloatAttribute("distance", &distance);
	p_element->QueryFloatAttribute("life", &life);
	p_element->Attribute("reflectionCounter", &reflectionCounter);

	// Any level file can carry these, and it may be anybody's. onUpdate()
	// walks a flight four pixels at a time, so a speed too large to count down
	// in fours would never finish a tick; a cannon fires at CANNON_SPEED and a
	// reflection only slows a shot down. The direction is not normalised, as a
	// bounce halves it on purpose, but it is held to a length a shot can have,
	// and the position to around the level, since both end in an int. Around,
	// because a shot fading after it left the grid stands outside it and a
	// saved game keeps it there; one further out is let go at once.
	speed = isFiniteFloat(speed) && speed > 0.0f ? min(speed, CANNON_SPEED) : 0.0f;
	if(!isFiniteFloat(velocity.x) || !isFiniteFloat(velocity.y) ||
	   fabsf(velocity.x) > 2.0f || fabsf(velocity.y) > 2.0f)
	{
		velocity = Vec2f(0.0f, 0.0f);
		speed = 0.0f;
	}

	const float w = Level::WIDTH * 16.0f, h = Level::HEIGHT * 16.0f;
	if(!isFiniteFloat(positionInPixels.x) || !isFiniteFloat(positionInPixels.y) ||
	   positionInPixels.x < -w || positionInPixels.x >= 2.0f * w ||
	   positionInPixels.y < -h || positionInPixels.y >= 2.0f * h)
	{
		positionInPixels = cell;
		life = 0.001f;
	}
}