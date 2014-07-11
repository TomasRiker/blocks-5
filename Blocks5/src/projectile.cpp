#include "pch.h"
#include "projectile.h"
#include "engine.h"
#include "tileset.h"
#include "particlesystem.h"
#include "linedrawer.h"

Projectile::Projectile(Level& level,
					   const Vec2d& positionInPixels,
					   const Vec2d& velocity) : Object(level, 0)
{
	type = "Projectile";
	warpTo(Vec2i(0, 0));
	flags = OF_PROXY | OF_NO_SHADOW;
	this->positionInPixels = positionInPixels;
	this->velocity = velocity;
	speed = velocity.length();
	this->velocity.normalize();
	distance = 0.0;
	life = 1.0;
	reflectionCounter = 3;
}

Projectile::~Projectile()
{
}

void Projectile::onRender(int layer,
						  const Vec4d& color)
{
	if(layer == 16 && life > 0.0)
	{
		double traceLength = min(distance, 0.035 * speed);

		glDisable(GL_TEXTURE_2D);

		// Glühen rendern
		Engine& engine = Engine::inst();
		engine.setBlendFunc(GL_SRC_ALPHA, GL_ONE, GL_ONE, GL_ONE);
		LineDrawer line;
		line.addPoint(positionInPixels - traceLength * velocity);
		line.addPoint(positionInPixels);
		line.setWidth(5.0f);
		line.setColor(Vec4d(1.0, 1.0, 1.0, 0.1 * min(life, 1.0)));
		line.draw();
		engine.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);

		// Projektil rendern
		line.setWidth(2.0f);
		line.setColor(Vec4d(1.0, 0.75, 0.1, life));
		line.draw();
		glPointSize(2.0f);
		glBegin(GL_POINTS);
		glVertex2dv(positionInPixels);
		glEnd();

		glEnable(GL_TEXTURE_2D);
	}
	else if(layer == 18)
	{
		// Projektil leuchten lassen
		glPushMatrix();
		glTranslated(positionInPixels.x - 8.0, positionInPixels.y - 8.0, 0.0);
		double s = fabs(life);
		level.renderShine(0.5 * s, 0.4 * s);
		glPopMatrix();
	}
}

void Projectile::onUpdate()
{
	if(life == 1.0)
	{
		double lengthToProcess = 0.02 * speed;
		while(lengthToProcess > 0.0)
		{
			double step = min(lengthToProcess, 4.0);
			lengthToProcess -= step;

			positionInPixels += step * velocity;
			distance += step;

			if(!level.isValidPosition(positionInPixels / 16))
			{
				life = 0.999;
				break;
			}
			else
			{
				Object* p_objectHit = 0;
				Vec2i tileHit;
				bool bounce = false;
				bool destroyed = false;
				bool reflected = false;
				Vec4d debrisColor;
				Vec2d hitPosition;

				if(distance >= 8.0 && !level.isFreeAt2(positionInPixels, 0, &p_objectHit, &tileHit, 64.0))
				{
					if(p_objectHit)
					{
						hitPosition = Vec2d(7.5, 7.5) + p_objectHit->getShownPositionInPixels();

						if(p_objectHit->reflectProjectile(velocity))
						{
							reflected = true;
							reflectionCounter--;
							distance = 0.0;
							Engine::inst().playSound("ricochet.ogg", false, 0.2);

							if(reflectionCounter < 0)
							{
								// Dieses Geschoss wurde schon zu oft reflektiert!
								Vec2d perp(-velocity.y, velocity.x);
								velocity += random(-0.25, 0.25) * perp;
								life = 0.999;
							}
						}

						if(!reflected)
						{
							if(p_objectHit->getFlags() & OF_DESTROYABLE)
							{
								p_objectHit->disappear(0.075);
								destroyed = true;
								debrisColor = p_objectHit->getDebrisColor();
							}
							else
							{
								bounce = true;
							}
						}
					}
					else
					{
						hitPosition = Vec2d(7.5, 7.5) + tileHit * 16;

						// Ist das Tile zerstörbar?
						int tileID = level.getTileAt(1, tileHit);
						const TileSet::TileInfo& tileInfo = level.getTileSet()->getTileInfo(tileID);
						if(tileInfo.type == 2)
						{
							level.setTileAt(1, tileHit, 0);
							destroyed = true;
							debrisColor = tileInfo.debrisColor;
						}
						else
						{
							bounce = true;
						}
					}

					ParticleSystem* p_particleSystem = level.getParticleSystem();
					ParticleSystem::Particle p;

					// Staub
					for(int i = 0; i < 30; i++)
					{
						p.lifetime = random(25, 50);
						p.damping = 0.99f;
						p.gravity = -0.005f;
						p.positionOnTexture = Vec2b(0, 0);
						p.sizeOnTexture = Vec2b(16, 16);
						p.position = positionInPixels;
						const double r = random(0.0, 6.283);
						p.velocity = random(0.25, 1.0) * Vec2d(sin(r), cos(r));
						double c = random(0.75, 1.0);
						p.color = Vec4d(c, c, c, random(0.15, 0.2));
						p.deltaColor = -p.color / static_cast<float>(p.lifetime);
						p.rotation = random(0.0f, 10.0f);
						p.deltaRotation = random(-0.1f, 0.1f);
						p.size = random(0.2f, 0.3f);
						p.deltaSize = random(0.01f, 0.05f);
						p_particleSystem->addParticle(p);
					}

					// glühende Partikel
					for(int i = 0; i < 20; i++)
					{
						p.lifetime = random(80, 120);
						p.damping = 0.9f;
						p.gravity = 0.1f;
						p.positionOnTexture = Vec2b(32, 32);
						p.sizeOnTexture = Vec2b(16, 16);
						p.position = positionInPixels;
						const double r = random(0.0, 6.283);
						p.velocity = random(3.0, 6.0) * Vec2d(sin(r), cos(r));
						p.color = Vec4d(random(0.5, 1.0), random(0.5, 1.0), 0.0, 0.9);
						p.deltaColor = Vec4d(0.5, 0.0, 0.0, -p.color.a / p.lifetime);
						p.rotation = random(0.0f, 10.0f);
						p.deltaRotation = random(-0.1f, 0.1f);
						p.size = random(0.1f, 0.2f);
						p.deltaSize = random(-0.01f, -0.005f);
						p_particleSystem->addParticle(p);
					}

					if(destroyed)
					{
						// Das Geschoss hat ein Objekt oder ein Tile zerstört.

						// Trümmer
						int n = random(30, 40);
						for(int i = 0; i < n; i++)
						{
							p.lifetime = random(40, 70);
							p.damping = 0.9f;
							p.gravity = 0.1f;
							p.positionOnTexture = Vec2b(96, 0);
							p.sizeOnTexture = Vec2b(16, 16);
							p.position = hitPosition + Vec2i(random(-5, 5), random(-5, 5));
							const double r = random(0.0, 6.283);
							p.velocity = random(2.0, 5.0) * Vec2d(sin(r), cos(r));
							p.color = debrisColor + Vec4d(random(-0.1, 0.1), random(-0.1, 0.1), random(-0.1, 0.1), random(0.3, 0.5));
							p.deltaColor = Vec4d(0.0, 0.0, 0.0, -0.5 * -p.color.a / p.lifetime);
							p.rotation = random(0.0f, 10.0f);
							p.deltaRotation = random(-0.1f, 0.1f);
							p.size = random(0.5f, 1.0f);
							p.deltaSize = -p.size / p.lifetime;
							p_particleSystem->addParticle(p);
						}

						Engine::inst().playSound("destroy.ogg", false, 0.1);

						velocity = Vec2d(0.0, 0.0);
						life = -2.0;
					}

					if(bounce)
					{
						// Das Geschoss soll abprallen.
						Vec2d perp(-velocity.y, velocity.x);
						velocity *= -0.5;
						velocity += random(-0.25, 0.25) * perp;

						Engine::inst().playSound("ricochet.ogg", false, 0.2);

						life = 0.999;
					}

					if(reflected)
					{
						// Geschwindigkeit verringern
						speed *= 0.8;

						// Position korrigieren
						positionInPixels = hitPosition;

						// das Geschoss ein bisschen weiterbewegen, damit es nicht dasselbe Objekt noch einmal trifft
						distance = 0.0;
						Object* p_newObjectHit;
						do
						{
							positionInPixels += velocity;
							distance += 1.0;

							p_newObjectHit = 0;
							level.isFreeAt2(positionInPixels, 0, &p_newObjectHit, &tileHit, 64.0);
						} while(p_newObjectHit == p_objectHit);
					}

					if(destroyed || bounce || reflected) break;
				}
			}
		}
	}
	else
	{
		positionInPixels += 0.02 * speed * velocity;
		distance += 0.02 * speed;

		// Das Projektil verschwindet langsam.
		if(life > 0.0)
		{
			life -= 0.02 * 5.0;
			if(life <= 0.0) disappear(0.0);
		}
		else if(life < 0.0)
		{
			life += 0.02 * 5.0;
			if(life >= 0.0) disappear(0.0);
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

	p_element->Attribute("positionInPixelsX", &positionInPixels.x);
	p_element->Attribute("positionInPixelsY", &positionInPixels.y);
	p_element->Attribute("velocityX", &velocity.x);
	p_element->Attribute("velocityY", &velocity.y);
	p_element->Attribute("speed", &speed);
	p_element->Attribute("distance", &distance);
	p_element->Attribute("life", &life);
	p_element->Attribute("reflectionCounter", &reflectionCounter);
}