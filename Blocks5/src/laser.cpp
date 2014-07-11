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
	warpTo(position);
	flags = OF_MASSIVE | OF_FIXED | OF_DESTROYABLE | OF_TRANSPORTABLE;
	destroyTime = 125;
	this->dir = dir;
	counter = 0;
	on = 0.0;

	if(!level.isInEditor())
	{
		numInstances++;

		if(numInstances == 1)
		{
			// Das ist die erste Instanz. Soundinstanz erzeugen und pausieren.
			Sound* p_sound = Manager<Sound>::inst().request("laser.ogg");
			p_soundInst = p_sound->createInstance();
			p_sound->release();

			if(p_soundInst)
			{
				p_soundInst->setVolume(0.0);
				p_soundInst->setPitch(0.1);
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
			// Das war die letzte Instanz. Sound stoppen.
			p_soundInst->stop();
			p_soundInst = 0;
			soundChanged = false;
		}
	}
}

void Laser::onRender(int layer,
					 const Vec4d& color)
{
	Vec2i positionOnTexture;
	if(level.isElectricityOn()) positionOnTexture = Vec2i(32, 192);
	else positionOnTexture = Vec2i(0, 192);

	Vec2i sp = getShownPositionInPixels();

	if(layer == 1)
	{
		// Laser rendern
		Engine::inst().renderSprite(Vec2i(0, 0), positionOnTexture, Vec2i(16, 16), color, false, 90.0 * dir);
	}
	else if(layer == 16 || layer == 17)
	{
		if(on > 0.0 && !beam.empty())
		{
			Vec2d oldDir(0.0);
			Vec2d oldP(0.0);
			Vec2d dir;
			Vec2d p;

			line.clear();

			std::list<Vec2d>::const_iterator last = beam.end();
			last--;
			for(std::list<Vec2d>::const_iterator i = beam.begin(); i != beam.end(); ++i)
			{
				oldP = p;
				oldDir = dir;
				p = *i;
				dir = p - oldP;

				if(i == beam.begin() || i == last || dir != oldDir)
				{
					line.addPoint(p);
				}
			}

			// inneren und äußeren Strahl rendern
			glPushMatrix();
			glTranslated(-sp.x, -sp.y, 0.0);
			glDisable(GL_TEXTURE_2D);

			double x = static_cast<double>(counter) * 0.8;
			Vec4d color;
			if(layer == 16) color = Vec4d(1.0, 0.25, 0.0, on * deathCountDown * (0.2 + 0.05 * sin(x)));
			else color = Vec4d(0.0, 0.25, 0.0, 0.4 * on * deathCountDown * (0.2 + 0.05 * sin(x)));
			line.setWidth(6.5f);
			line.setColor(color);
			line.draw();
			glPointSize(7.0f);
			glBegin(GL_POINTS);
			glColor4dv(color);
			glVertex2dv(p);
			glEnd();

			if(layer == 16) color = Vec4d(1.0, random(0.6, 0.65), 0.0, on * deathCountDown * (0.9 + 0.1 * cos(x)));
			else color = Vec4d(0.0, random(0.6, 0.65), 0.0, 0.4 * on * deathCountDown * (0.9 + 0.1 * cos(x)));
			line.setWidth(1.5f);
			line.setColor(color);
			line.draw();
			glPointSize(3.0f);
			glBegin(GL_POINTS);
			glColor4dv(color);
			glVertex2dv(p);
			glEnd();

			glEnable(GL_TEXTURE_2D);
			glPopMatrix();
		}
	}
	else if(layer == 18)
	{
		if(on > 0.0)
		{
			int j = 0;
			for(std::list<Vec2d>::const_iterator i = beam.begin(); i != beam.end(); ++i)
			{
				if(!(j % 4))
				{
					Vec2d p = *i - sp;
					glPushMatrix();
					glTranslated(p.x - 7.5, p.y - 7.5, 0.0);
					level.renderShine(0.25, on * (0.4 + random(-0.05, 0.05)));
					glPopMatrix();
				}

				j++;
			}
		}
	}
}

void Laser::onUpdate()
{
	soundChanged = false;
	if(destroyTime < 5) destroyTime--;

	if(!destroyTime)
	{
		Engine::inst().playSound("vaporize.ogg", false, 0.15);

		// Trümmer
		ParticleSystem* p_particleSystem = level.getParticleSystem();
		ParticleSystem::Particle p;
		int n = random(50, 80);
		for(int i = 0; i < n; i++)
		{
			p.lifetime = random(60, 120);
			p.damping = 0.9f;
			p.gravity = -0.1f;
			p.positionOnTexture = Vec2b(96, 0);
			p.sizeOnTexture = Vec2b(16, 16);
			p.position = position * 16 + Vec2i(random(-2, 18), random(-2, 18));
			p.velocity = Vec2d(random(-0.2, 0.2), random(-0.2, 0.2));
			p.color = debrisColor + Vec4d(random(-0.1, 0.1), random(-0.1, 0.1), random(-0.1, 0.1), 0.0);
			p.deltaColor = Vec4d(0.0, 0.0, 0.0, -p.color.a / p.lifetime);
			p.rotation = random(0.0f, 10.0f);
			p.deltaRotation = random(-0.1f, 0.1f);
			p.size = random(0.5f, 1.5f);
			p.deltaSize = random(0.01f, 0.05f);
			p_particleSystem->addParticle(p);
		}

		disappear(0.2);
	}

	beam.clear();
	if(on > 0.0)
	{
		// Strahl berechnen
		Vec2i beamDir = numberToDir(dir);
		Vec2d beamPos = Vec2d(7.5, 7.5) + getShownPositionInPixels();
		Vec2i beamPosF;
		beam.push_back(beamPos);
		beamPos += beamDir;

		bool destroyed = false;
		bool infinity = false;
		Vec4d debrisColor;
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

			// Ist an dieser Stelle etwas?
			Object* p_obj = 0;
			Vec2i tileHit;
			if(z > 2 && !level.isFreeAt2(beamPos, 0, &p_obj, &tileHit))
			{
				if(p_obj)
				{
					// Ein Objekt versperrt den Weg.
					if(beamDir.x) beamPos.x = 7.5 + p_obj->getShownPositionInPixels().x;
					else if(beamDir.y) beamPos.y = 7.5 + p_obj->getShownPositionInPixels().y;
					beam.back() = beamPos;

					if(p_obj->reflectLaser(beamDir))
					{
						// OK, das Objekt hat den Laser umgelenkt!
						reflected = true;
					}
					else if(p_obj->getFlags() & OF_DESTROYABLE)
					{
						p_obj->setDestroyTime(p_obj->getDestroyTime() - 1);
						if(!p_obj->getDestroyTime())
						{
							p_obj->disappear(0.2);
							destroyed = true;
							debrisColor = p_obj->getDebrisColor();
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
					// Ein Tile versperrt den Weg.
					int tileID = level.getTileAt(1, tileHit);
					const TileSet::TileInfo& tileInfo = level.getTileSet()->getTileInfo(tileID);
					if(tileInfo.type == 2)
					{
						level.setTileDestroyTimeAt(1, tileHit, level.getTileDestroyTimeAt(1, tileHit) - 1);
						if(!level.getTileDestroyTimeAt(1, tileHit))
						{
							level.setTileAt(1, tileHit, 0);
							destroyed = true;
							debrisColor = tileInfo.debrisColor;
						}
					}

					if(beamDir.x) beamPos.x = 7.5 + tileHit.x * 16;
					else if(beamDir.y) beamPos.y = 7.5 + tileHit.y * 16;
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
				// Rauch
				p.lifetime = random(70, 100);
				p.damping = 0.99f;
				p.gravity = 0.005f;
				p.positionOnTexture = Vec2b(0, 0);
				p.sizeOnTexture = Vec2b(16, 16);
				p.position = beamPos;
				p.velocity = Vec2d(random(-0.25, 0.25), -1.0);
				p.color = Vec4d(0.0, 0.0, 0.0, on * 0.2);
				p.deltaColor = Vec4d(0.0, 0.0, 0.0, -p.color.a / p.lifetime);
				p.rotation = random(0.0f, 10.0f);
				p.deltaRotation = random(-0.1f, 0.1f);
				p.size = random(0.3f, 0.5f);
				p.deltaSize = random(0.01f, 0.02f);
				p_particleSystem->addParticle(p);
			}

			if(!(counter % 4))
			{
				// glühende Partikel
				p.lifetime = random(80, 120);
				p.damping = 0.9f;
				p.gravity = 0.1f;
				p.positionOnTexture = Vec2b(32, 32);
				p.sizeOnTexture = Vec2b(16, 16);
				p.position = beamPos + Vec2f(random(6.0f, 10.0f), random(6.0f, 10.0f));
				const double r = random(0.0, 6.283);
				p.velocity = random(3.0, 6.0) * Vec2d(sin(r), cos(r));
				p.color = Vec4d(random(0.5, 1.0), random(0.5, 1.0), 0.0, on * 0.9);
				p.deltaColor = Vec4d(0.5, 0.0, 0.0, -p.color.a / p.lifetime);
				p.rotation = random(0.0f, 10.0f);
				p.deltaRotation = random(-0.1f, 0.1f);
				p.size = random(0.1f, 0.2f);
				p.deltaSize = random(-0.01f, -0.005f);
				if(random() % 2) p_particleSystem->addParticle(p);
				else p_fireParticleSystem->addParticle(p);
			}

			if(destroyed)
			{
				Engine::inst().playSound("vaporize.ogg", false, 0.15);

				// Trümmer
				int n = random(50, 80);
				for(int i = 0; i < n; i++)
				{
					p.lifetime = random(60, 120);
					p.damping = 0.9f;
					p.gravity = -0.1f;
					p.positionOnTexture = Vec2b(96, 0);
					p.sizeOnTexture = Vec2b(16, 16);
					p.position = beamPosF * 16 + Vec2i(random(-2, 18), random(-2, 18));
					p.velocity = Vec2d(random(-0.2, 0.2), random(-0.2, 0.2));
					p.color = debrisColor + Vec4d(random(-0.1, 0.1), random(-0.1, 0.1), random(-0.1, 0.1), 0.0);
					p.deltaColor = Vec4d(0.0, 0.0, 0.0, -p.color.a / p.lifetime);
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

	if(level.isElectricityOn()) on += 0.2;
	else on -= 0.2;
	on = clamp(on, 0.0, 1.0);
}

void Laser::onElectricitySwitch(bool on)
{
	if(soundChanged) return;
	if(!p_soundInst) return;

	// Sound kontrollieren
	if(on)
	{
		p_soundInst->resume();
		p_soundInst->slideVolume(0.25, 0.2);
		p_soundInst->slidePitch(1.0, 0.2);
	}
	else
	{
		p_soundInst->slideVolume(-1.0, 0.1);
		p_soundInst->slidePitch(0.1, 0.1);
	}

	soundChanged = true;
}

void Laser::frameBegin()
{
	Object::frameBegin();

	for(std::list<Vec2d>::const_iterator it = beam.begin();
		it != beam.end();
		++it)
	{
		// Gegner sollten Laser meiden ...
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