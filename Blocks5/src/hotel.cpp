#include "pch.h"
#include "hotel.h"
#include "engine.h"
#include "particlesystem.h"
#include "player.h"

Hotel* Hotel::p_hotelToSave = 0;

Hotel::Hotel(Level& level,
			 const Vec2i& position) : Object(level, 1)
{
	warpTo(position);
	flags = OF_FIXED;
	state = -1;
}

Hotel::~Hotel()
{
}

void Hotel::onRender(int layer,
					 const Vec4d& color)
{
	if(layer == 1)
	{
		// Hotel rendern
		Engine::inst().renderSprite(Vec2i(0, 0), positionOnTexture, Vec2i(16, 16), color);
	}
	else if(layer == 18)
	{
		level.renderShine(0.4, 0.4 + random(-0.05, 0.05));
	}
}

void Hotel::onUpdate()
{
	// Rauch
	ParticleSystem* p_particleSystem = level.getParticleSystem();
	ParticleSystem::Particle p;
	p.lifetime = random(30, 40);
	p.damping = 0.99f;
	p.gravity = 0.005f;
	p.positionOnTexture = Vec2b(0, 0);
	p.sizeOnTexture = Vec2b(16, 16);
	p.position = position * 16 + Vec2i(random(11, 13), 2);
	p.velocity = Vec2d(random(-0.25, 0.25), -0.5);
	p.color = Vec4d(0.25, 0.25, 0.25, random(0.15, 0.25));
	p.deltaColor = Vec4d(0.0, 0.0, 0.0, -p.color.a / p.lifetime);
	p.rotation = random(0.0f, 10.0f);
	p.deltaRotation = random(-0.1f, 0.1f);
	p.size = random(0.2f, 0.3f);
	p.deltaSize = random(0.01f, 0.05f);
	p_particleSystem->addParticle(p);

	// Spieler da?
	Object* p_obj = level.getFrontObjectAt(position);
	if(p_obj == level.getActivePlayer())
	{
		if(state == 0) state = 1;

		if(state == 1)
		{
			say("$G_HOTEL_WELCOME", 0.5);
			p_hotelToSave = this;
		}
	}
	else
	{
		if(p_hotelToSave == this) p_hotelToSave = 0;

		bool reset = false;
		if(!p_obj) reset = true;
		else if(p_obj->getType() != "Player") reset = true;

		if(reset) state = 0;
	}
}

void Hotel::onSave()
{
	say("$G_HOTEL_GOODBYE", 2.0);

	state = -1;
	p_hotelToSave = 0;

	Engine::inst().playSound("hotel.ogg", false, 0.0, 100);
}