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
	renderLayers = RL_MAIN | RL_LIGHT;
	warpTo(position);
	flags = OF_MASSIVE | OF_FIXED | OF_DESTROYABLE | OF_TRANSPORTABLE;
	destroyTime = 125;
	this->color = color;
	this->dir = dir;
	shownDir = static_cast<float>(dir);
	reload = 0;
}

Cannon::~Cannon()
{
}

void Cannon::updateSprites()
{
	// Base and barrel. The barrel turns smoothly with shownDir, the base
	// does not.
	const Vec4f realColor = getStdColor(this->color);
	sprites.add(Vec2i(96, 416), realColor);

	int frame;
	if(reload <= 25) frame = 0;
	else if(reload <= 30) frame = 2;
	else frame = 1;
	sprites.add(Vec2i(128 + frame * 32, 416), realColor).rotation = 90.0f * shownDir;
}

void Cannon::onRender(RenderLayer layer,
					  const Vec4f& color)
{
	if(layer == RL_MAIN) Engine::inst().renderSprites(sprites, color);
	else if(layer == RL_LIGHT)
	{
		if(reload)
		{
			float s = static_cast<float>(reload) / 100;
			s *= s;
			s *= s;
			s *= s;
			s *= s;
			const Vec2f up = 6 * intToDir(dir);
			level.renderShine(s * 2.0f, s * 2.0f, up);
		}
	}
}

void Cannon::onUpdate()
{
	// aim the cannon
	float dd = static_cast<float>(dir) - shownDir;
	if(dd > 2.0f) shownDir += 4.0f;
	else if(dd < -2.0f) shownDir -= 4.0f;
	dd = static_cast<float>(dir) - shownDir;
	if(dd > 0.03f) shownDir += 0.03f;
	else if(dd < -0.03f) shownDir -= 0.03f;
	else shownDir = static_cast<float>(dir);

	if(reload)
	{
		Vec2f up = intToDir(dir);

		ParticleSystem* p_particleSystem = level.getParticleSystem();
		ParticleSystem::Particle p;

		// smoke
		p.lifetime = static_cast<ushort>(random(25, 50));
		p.damping = 0.99f;
		p.gravity = 0.005f;
		p.positionOnTexture = Vec2b(0, 0);
		p.sizeOnTexture = Vec2b(16, 16);
		p.position = Vec2f(7.5f, 7.5f) + 6.0f * up + shownPosition * 16.0f;
		p.velocity = Vec2f(random(-0.25f, 0.25f), -1.0f);
		float c = random(0.75f, 1.0f);
		p.color = Vec4f(c, c, c, random(0.15f, 0.2f));
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
		shownDir = static_cast<float>(dir);
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

	// QueryFloatAttribute rather than Attribute() and sscanf: it leaves
	// shownDir alone where the attribute is missing, whereas Attribute()
	// would hand a null pointer straight into sscanf there.
	p_element->QueryFloatAttribute("shownDir", &shownDir);
}

uint Cannon::getColor() const
{
	return color;
}

bool Cannon::fire()
{
	// Not reloaded yet?
	if(reload) return false;

	// Not finished aiming yet?
	float dd = static_cast<float>(dir) - shownDir;
	if(fabsf(dd) > 0.1f) return false;

	// compute the direction vectors
	Vec2f up, right;
	switch(dir % 4)
	{
	case 0: up = Vec2f(0.0f, -1.0f), right = Vec2f(1.0f, 0.0f); break;
	case 1: up = Vec2f(1.0f, 0.0f), right = Vec2f(0.0f, 1.0f); break;
	case 2: up = Vec2f(0.0f, 1.0f), right = Vec2f(-1.0f, 0.0f); break;
	case 3: up = Vec2f(-1.0f, 0.0f), right = Vec2f(0.0f, -1.0f); break;
	}

	// fire the projectile
	new Projectile(level, Vec2f(7.5f, 7.5f) + 5.0f * up + shownPosition * 16.0f, up * 1200.0f);

	ParticleSystem* p_particleSystem = level.getParticleSystem();
	ParticleSystem::Particle p;

	// fire/smoke forward and to the sides
	for(int i = 0; i < 100; i++)
	{
		p.lifetime = static_cast<ushort>(random(5, 10));
		p.damping = 0.99f;
		p.gravity = 0.005f;
		p.positionOnTexture = Vec2b(64, 0);
		p.sizeOnTexture = Vec2b(16, 16);
		p.position = Vec2f(7.5f, 7.5f) + 5.0f * up + shownPosition * 16.0f;
		p.velocity = random(4.0f, 7.0f) * up + Vec2f(random(-1.0f, 1.0f), 0.0f);
		p.color = Vec4f(1.0f, 1.0f, 1.0f, random(0.15f, 0.25f));
		p.deltaColor = -p.color / static_cast<float>(p.lifetime);
		p.rotation = random(0.0f, 10.0f);
		p.deltaRotation = random(-0.1f, 0.1f);
		p.size = random(0.25f, 0.35f);
		p.deltaSize = random(0.01f, 0.05f);
		p_particleSystem->addParticle(p);

		p.velocity = random(4.0f, 7.0f) * right + Vec2f(0.0f, random(-1.0f, 1.0f));
		p_particleSystem->addParticle(p);

		p.velocity = random(4.0f, 7.0f) * -right + Vec2f(0.0f, random(-1.0f, 1.0f));
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