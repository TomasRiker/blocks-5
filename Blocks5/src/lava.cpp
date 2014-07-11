#include "pch.h"
#include "lava.h"
#include "engine.h"
#include "particlesystem.h"

Lava::Lava(Level& level,
		   const Vec2i& position,
		   int dir) : Object(level, 401)
{
	warpTo(position);
	flags = OF_FIXED | OF_DONT_FALL | OF_NO_SHADOW;
	this->dir = dir;
	anim = 0;
}

Lava::~Lava()
{
}

void Lava::onRender(int layer,
					const Vec4d& color)
{
	if(layer == 736)
	{
		Engine& engine = Engine::inst();

		uint tl = level.getAIFlags(position + Vec2i(-1, -1)) & 4;
		uint t = level.getAIFlags(position + Vec2i(0, -1)) & 4;
		uint tr = level.getAIFlags(position + Vec2i(1, -1)) & 4;
		uint l = level.getAIFlags(position + Vec2i(-1, 0)) & 4;
		uint r = level.getAIFlags(position + Vec2i(1, 0)) & 4;
		uint bl = level.getAIFlags(position + Vec2i(-1, 1)) & 4;
		uint b = level.getAIFlags(position + Vec2i(0, 1)) & 4;
		uint br = level.getAIFlags(position + Vec2i(1, 1)) & 4;

		if(!l && !t && !tl)	engine.renderSprite(Vec2i(0, 0), Vec2i(0, 0), Vec2i(16, 16), Vec4d(1.0));
		if(!t)				engine.renderSprite(Vec2i(0, 0), Vec2i(16, 0), Vec2i(16, 16), Vec4d(1.0));
		if(!r && !t && !tr)	engine.renderSprite(Vec2i(0, 0), Vec2i(32, 0), Vec2i(16, 16), Vec4d(1.0));
		if(!l)				engine.renderSprite(Vec2i(0, 0), Vec2i(0, 16), Vec2i(16, 16), Vec4d(1.0));
		if(!r)				engine.renderSprite(Vec2i(0, 0), Vec2i(32, 16), Vec2i(16, 16), Vec4d(1.0));
		if(!l && !b && !bl)	engine.renderSprite(Vec2i(0, 0), Vec2i(0, 32), Vec2i(16, 16), Vec4d(1.0));
		if(!b)				engine.renderSprite(Vec2i(0, 0), Vec2i(16, 32), Vec2i(16, 16), Vec4d(1.0));
		if(!r && !b && !br)	engine.renderSprite(Vec2i(0, 0), Vec2i(32, 32), Vec2i(16, 16), Vec4d(1.0));

		if(l && t && !tl)	engine.renderSprite(Vec2i(0, 0), Vec2i(64, 16), Vec2i(16, 16), Vec4d(1.0));
		if(l && b && !bl)	engine.renderSprite(Vec2i(0, 0), Vec2i(64, 0), Vec2i(16, 16), Vec4d(1.0));
		if(r && t && !tr)	engine.renderSprite(Vec2i(0, 0), Vec2i(48, 16), Vec2i(16, 16), Vec4d(1.0));
		if(r && b && !br)	engine.renderSprite(Vec2i(0, 0), Vec2i(48, 0), Vec2i(16, 16), Vec4d(1.0));
	}
	else if(layer == 737 || layer == 738)
	{
		// Lava rendern
		Vec2d shift(2.0 * sin(0.1 * anim), 3.0 * cos(0.05 * anim));
		double a[4];
		if(layer == 737) getAlpha1(position, a);
		else if(layer == 738) getAlpha2(position, a);

		int numPasses = 1;
		if(dir > 3) numPasses = 2;

		for(int i = 0; i < numPasses; i++)
		{
			double tl = 1.0, tr = 1.0, br = 1.0, bl = 1.0;
			int ndir = dir;

			if(i == 1)
			{
				switch(dir)
				{
				case 4: ndir += 3, tl = 1.0, tr = 1.0, br = 0.0, bl = 0.0; break;
				case 5: ndir += 3, tl = 0.0, tr = 1.0, br = 1.0, bl = 0.0; break;
				case 6: ndir += 3, tl = 0.0, tr = 0.0, br = 1.0, bl = 1.0; break;
				case 7: ndir += 3, tl = 1.0, tr = 0.0, br = 0.0, bl = 1.0; break;

				case 8: ndir += 1, tl = 1.0, tr = 1.0, br = 0.0, bl = 0.0; break;
				case 9: ndir += 1, tl = 0.0, tr = 1.0, br = 1.0, bl = 0.0; break;
				case 10: ndir += 1, tl = 0.0, tr = 0.0, br = 1.0, bl = 1.0; break;
				case 11: ndir += 1, tl = 1.0, tr = 0.0, br = 0.0, bl = 1.0; break;

				case 12: ndir += 2, tl = 0.0, tr = 1.0, br = 1.0, bl = 0.0; break;
				case 13: ndir += 2, tl = 0.0, tr = 0.0, br = 1.0, bl = 1.0; break;
				case 14: ndir += 2, tl = 1.0, tr = 0.0, br = 0.0, bl = 1.0; break;
				case 15: ndir += 2, tl = 1.0, tr = 1.0, br = 0.0, bl = 0.0; break;

				case 16: ndir += 2, tl = 1.0, tr = 0.0, br = 0.0, bl = 1.0; break;
				case 17: ndir += 2, tl = 1.0, tr = 1.0, br = 0.0, bl = 0.0; break;
				case 18: ndir += 2, tl = 0.0, tr = 1.0, br = 1.0, bl = 0.0; break;
				case 19: ndir += 2, tl = 0.0, tr = 0.0, br = 1.0, bl = 1.0; break;
				}
			}

			Vec2d t;
			switch(ndir % 4)
			{
			case 0: t = Vec2i(0, anim); break;
			case 1: t = Vec2i(-anim, 0); break;
			case 2: t = Vec2i(0, -anim); break;
			case 3: t = Vec2i(anim, 0); break;
			}

			t += shift;

			if(layer == 737)
			{
				glBegin(GL_QUADS);
				glColor4d(1.0, 1.0, 1.0, a[0] * tl);
				glTexCoord2d(t.x, t.y);
				glVertex2d(0, 0);
				glColor4d(1.0, 1.0, 1.0, a[1] * tr);
				glTexCoord2d(t.x + 16.0, t.y);
				glVertex2i(16, 0);
				glColor4d(1.0, 1.0, 1.0, a[2] * br);
				glTexCoord2d(t.x + 16.0, t.y + 16.0);
				glVertex2i(16, 16);
				glColor4d(1.0, 1.0, 1.0, a[3] * bl);
				glTexCoord2d(t.x, t.y + 16.0);
				glVertex2i(0, 16);
				glEnd();
			}

			t /= 2.0;

			if(layer == 738)
			{
				glBegin(GL_QUADS);
				glColor4d(1.0, 1.0, 1.0, a[0] * tl);
				glTexCoord2d(t.x, t.y);
				glVertex2d(0, 0);
				glColor4d(1.0, 1.0, 1.0, a[1] * tr);
				glTexCoord2d(t.x + 16.0, t.y);
				glVertex2i(16, 0);
				glColor4d(1.0, 1.0, 1.0, a[2] * br);
				glTexCoord2d(t.x + 16.0, t.y + 16.0);
				glVertex2i(16, 16);
				glColor4d(1.0, 1.0, 1.0, a[3] * bl);
				glTexCoord2d(t.x, t.y + 16.0);
				glVertex2i(0, 16);
				glEnd();
			}
		}
	}
	else if(layer == 18)
	{
		level.renderShine(0.75, 0.35 + random(-0.05, 0.05));
	}

	if(layer == 255)
	{
		// Fließrichtung anzeigen
		glPushAttrib(GL_ENABLE_BIT);
		glDisable(GL_TEXTURE_2D);
		glPushMatrix();
		glTranslated(7.5, 7.5, 0.0);
		glRotated(90.0 * (dir % 4), 0.0, 0.0, 1.0);

		glBegin(GL_LINES);
		glColor4d(1.0, 1.0, 1.0, 1.0);

		if(dir <= 3)
		{
			glVertex2i(0, 5);
			glVertex2i(0, -5);
			glVertex2i(0, -5);
			glVertex2i(-3, -2);
			glVertex2i(0, -5);
			glVertex2i(3, -2);
		}
		else if(dir <= 7)
		{
			glVertex2i(5, 5);
			glVertex2i(5, -5);
			glVertex2i(5, -5);
			glVertex2i(-5, -5);
			glVertex2i(-5, -5);
			glVertex2i(-2, -8);
			glVertex2i(-5, -5);
			glVertex2i(-2, -2);
		}
		else if(dir <= 11)
		{
			glVertex2i(-5, 5);
			glVertex2i(-5, -5);
			glVertex2i(-5, -5);
			glVertex2i(5, -5);
			glVertex2i(5, -5);
			glVertex2i(2, -8);
			glVertex2i(5, -5);
			glVertex2i(2, -2);
		}
		else if(dir <= 15)
		{
			glVertex2i(-5, 5);
			glVertex2i(-5, -5);
			glVertex2i(-5, -5);
			glVertex2i(-8, -2);
			glVertex2i(-5, -5);
			glVertex2i(-2, -2);

			glVertex2i(5, -5);
			glVertex2i(5, 5);
			glVertex2i(5, 5);
			glVertex2i(2, 2);
			glVertex2i(5, 5);
			glVertex2i(8, 2);
		}
		else if(dir <= 19)
		{
			glVertex2i(-5, -5);
			glVertex2i(-5, 5);
			glVertex2i(-5, 5);
			glVertex2i(-8, 2);
			glVertex2i(-5, 5);
			glVertex2i(-2, 2);

			glVertex2i(5, 5);
			glVertex2i(5, -5);
			glVertex2i(5, -5);
			glVertex2i(2, -2);
			glVertex2i(5, -5);
			glVertex2i(8, -2);
		}

		glEnd();

		glPopMatrix();
		glPopAttrib();
	}
}

void Lava::onUpdate()
{
	ParticleSystem* p_particleSystem = level.getParticleSystem();
	ParticleSystem::Particle p;

	if(!(random() % 20))
	{
		// Dampf
		p.lifetime = random(20, 30);
		p.damping = 0.9f;
		p.gravity = -0.04f;
		p.positionOnTexture = Vec2b(0, 0);
		p.sizeOnTexture = Vec2b(16, 16);
		p.position = position * 16 + Vec2i(random(0, 15), random(0, 15));
		p.velocity = Vec2d(random(-0.5, 0.5), random(-0.5, 0.5));
		p.color = Vec4d(random(0.9, 1.0), random(0.9, 1.0), random(0.9, 1.0), random(0.75, 1.0));
		p.deltaColor = Vec4d(0.0, 0.0, 0.0, -p.color.a / p.lifetime);
		p.rotation = random(0.0f, 10.0f);
		p.deltaRotation = random(-0.1f, 0.1f);
		p.size = random(0.75f, 1.2f);
		p.deltaSize = random(0.015f, 0.075f);
		p_particleSystem->addParticle(p);
	}

	// Aufzüge schützen die Objekte vor der Lava.
	bool elevatorFound = false;
	const std::vector<Object*> objectsOnMe = level.getObjectsAt(position);
	for(std::vector<Object*>::const_iterator i = objectsOnMe.begin(); i != objectsOnMe.end(); ++i)
	{
		if((*i)->getFlags() & OF_ELEVATOR)
		{
			elevatorFound = true;
			break;
		}
	}

	if(!elevatorFound)
	{
		// Befindet sich ein Objekt in der Lava?
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
					debrisColor = p_obj->getDebrisColor();

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
						p.position = p_obj->getPosition() * 16 + Vec2i(random(-2, 18), random(-2, 18));
						p.velocity = Vec2d(random(-0.2, 0.2), random(-0.2, 0.2));
						p.color = debrisColor + Vec4d(random(-0.1, 0.1), random(-0.1, 0.1), random(-0.1, 0.1), 0.0);
						p.deltaColor = Vec4d(0.0, 0.0, 0.0, -p.color.a / p.lifetime);
						p.rotation = random(0.0f, 10.0f);
						p.deltaRotation = random(-0.1f, 0.1f);
						p.size = random(0.5f, 1.5f);
						p.deltaSize = random(0.01f, 0.05f);
						p_particleSystem->addParticle(p);
					}
				}
			}
		}
	}

	anim++;
}

void Lava::frameBegin()
{
	Object::frameBegin();
	level.setAIFlag(position, 4);
}

bool Lava::changeInEditor(int mod)
{
	if(!mod)
	{
		int o = dir / 4;
		dir = 4 * o + ((dir + 1) % 4);
	}
	else
	{
		dir += 4;
	}

	dir %= 20;

	return true;
}

void Lava::saveAttributes(TiXmlElement* p_target)
{
	p_target->SetAttribute("dir", dir);
}

void Lava::getAlpha1(const Vec2i& where,
					 double* p_out)
{
	double po = 0.3 * (where.x + where.y);
	double x = 0.1 * anim;
	p_out[0] = 1.0 + 0.1 * sin(po + x);
	p_out[1] = 1.0 + 0.1 * sin(po + 0.3 + x);
	p_out[2] = 1.0 + 0.1 * sin(po + 0.6 + x);
	p_out[3] = 1.0 + 0.1 * sin(po + 0.3 + x);
}

void Lava::getAlpha2(const Vec2i& where,
					 double* p_out)
{
	double po = 0.3 * (where.x + where.y);
	double x = 0.1 * anim;
	p_out[0] = 0.5 + 0.5 * cos(po + x);
	p_out[1] = 0.5 + 0.5 * cos(po + 0.3 + x);
	p_out[2] = 0.5 + 0.5 * cos(po + 0.6 + x);
	p_out[3] = 0.5 + 0.5 * cos(po + 0.3 + x);
}