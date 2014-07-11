#include "pch.h"
#include "cannon.h"
#include "particlesystem.h"
#include "tileset.h"
#include "engine.h"
#include "projectile.h"

Cannon::Cannon(Level& level,
			   const Vec2i& position,
			   uint color,
			   int dir) : Object(level, 1)
{
	warpTo(position);
	flags = OF_MASSIVE | OF_FIXED | OF_DESTROYABLE | OF_TRANSPORTABLE;
	destroyTime = 125;
	debrisColor = getStdColor(color);
	debrisColor.a = 0.25;
	this->color = color;
	this->dir = dir;
	shownDir = dir;
	reload = 0;
}

Cannon::~Cannon()
{
}

void Cannon::onRender(int layer,
					  const Vec4d& color)
{
	if(layer == 1)
	{
		Engine& engine = Engine::inst();

		// Basis rendern
		Vec4d realColor = color * getStdColor(this->color);
		engine.renderSprite(Vec2i(0, 0), Vec2i(96, 416), Vec2i(16, 16), realColor);

		// Rohr rendern
		int frame;
		if(reload <= 25) frame = 0;
		else if(reload <= 30) frame = 2;
		else frame = 1;
		engine.renderSprite(Vec2i(0, 0), Vec2i(128 + frame * 32, 416), Vec2i(16, 16), realColor, false, 90.0 * shownDir);
	}
	else if(layer == 18)
	{
		if(reload)
		{
			double s = static_cast<double>(reload) / 100;
			s *= s;
			s *= s;
			s *= s;
			s *= s;
			Vec2d up = 6 * intToDir(dir);
			glPushMatrix();
			glTranslated(up.x, up.y, 0.0);
			level.renderShine(s * 2.0, s * 2.0);
			glPopMatrix();
		}
	}
}

void Cannon::onUpdate()
{
	// Kanone ausrichten
	double dd = static_cast<double>(dir) - shownDir;
	if(dd > 2.0) shownDir += 4.0;
	else if(dd < -2.0) shownDir -= 4.0;
	dd = static_cast<double>(dir) - shownDir;
	if(dd > 0.03) shownDir += 0.03;
	else if(dd < -0.03) shownDir -= 0.03;
	else shownDir = dir;

	if(reload)
	{
		Vec2d up = intToDir(dir);

		ParticleSystem* p_particleSystem = level.getParticleSystem();
		ParticleSystem::Particle p;

		// Rauch
		p.lifetime = random(25, 50);
		p.damping = 0.99f;
		p.gravity = 0.005f;
		p.positionOnTexture = Vec2b(0, 0);
		p.sizeOnTexture = Vec2b(16, 16);
		p.position = Vec2d(7.5, 7.5) + 6.0 * up + shownPosition * 16.0;
		p.velocity = Vec2d(random(-0.25, 0.25), -1.0);
		double c = random(0.75, 1.0);
		p.color = Vec4d(c, c, c, random(0.15, 0.2));
		p.deltaColor = -p.color / static_cast<float>(p.lifetime);
		p.rotation = random(0.0f, 10.0f);
		p.deltaRotation = random(-0.1f, 0.1f);
		p.size = random(0.2f, 0.3f);
		p.deltaSize = random(0.01f, 0.05f);
		p_particleSystem->addParticle(p);

		reload--;
	}
}

bool Cannon::changeInEditor(int mod)
{
	if(!mod)
	{
		color++;
		color %= 6;
	}
	else
	{
		dir++;
		dir %= 4;
		shownDir = dir;
	}

	return true;
}

void Cannon::saveAttributes(TiXmlElement* p_target)
{
	p_target->SetAttribute("color", color);
	p_target->SetAttribute("dir", dir);
}

void Cannon::saveExtendedAttributes(TiXmlElement* p_target)
{
	Object::saveExtendedAttributes(p_target);

	char temp[256] = "";
	sprintf(temp, "%f", shownDir);
	p_target->SetAttribute("shownDir", temp);
}

void Cannon::loadExtendedAttributes(TiXmlElement* p_element)
{
	Object::loadExtendedAttributes(p_element);

	const char* p_temp;
	p_temp = p_element->Attribute("shownDir");
	sscanf(p_temp, "%f", &shownDir);
}

uint Cannon::getColor() const
{
	return color;
}

bool Cannon::fire()
{
	// Noch nicht nachgeladen?
	if(reload) return false;

	// Noch nicht fertig ausgerichtet?
	double dd = static_cast<double>(dir) - shownDir;
	if(fabs(dd) > 0.1) return false;

	// Richtungsvektoren berechnen
	Vec2d up, right;
	switch(dir % 4)
	{
	case 0: up = Vec2d(0.0, -1.0), right = Vec2d(1.0, 0.0); break;
	case 1: up = Vec2d(1.0, 0.0), right = Vec2d(0.0, 1.0); break;
	case 2: up = Vec2d(0.0, 1.0), right = Vec2d(-1.0, 0.0); break;
	case 3: up = Vec2d(-1.0, 0.0), right = Vec2d(0.0, -1.0); break;
	}

	// Projektil abfeuern
	new Projectile(level, Vec2d(7.5, 7.5) + 5.0 * up + shownPosition * 16.0, up * 1200.0);

	ParticleSystem* p_particleSystem = level.getParticleSystem();
	ParticleSystem::Particle p;

	// Feuer/Rauch nach vorn und zu den Seiten
	for(int i = 0; i < 100; i++)
	{
		p.lifetime = random(5, 10);
		p.damping = 0.99f;
		p.gravity = 0.005f;
		p.positionOnTexture = Vec2b(64, 0);
		p.sizeOnTexture = Vec2b(16, 16);
		p.position = Vec2d(7.5, 7.5) + 5.0 * up + shownPosition * 16.0;
		p.velocity = random(4.0, 7.0) * up + Vec2d(random(-1.0, 1.0), 0.0);
		p.color = Vec4d(1.0, 1.0, 1.0, random(0.15, 0.25));
		p.deltaColor = -p.color / static_cast<float>(p.lifetime);
		p.rotation = random(0.0f, 10.0f);
		p.deltaRotation = random(-0.1f, 0.1f);
		p.size = random(0.25f, 0.35f);
		p.deltaSize = random(0.01f, 0.05f);
		p_particleSystem->addParticle(p);

		p.velocity = random(4.0, 7.0) * right + Vec2d(0.0, random(-1.0, 1.0));
		p_particleSystem->addParticle(p);

		p.velocity = random(4.0, 7.0) * -right + Vec2d(0.0, random(-1.0, 1.0));
		p_particleSystem->addParticle(p);
	}

	reload = 100;
	return true;
}

void Cannon::rotate()
{
	dir++;
	dir %= 4;
}