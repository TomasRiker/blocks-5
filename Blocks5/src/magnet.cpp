#include "pch.h"
#include "magnet.h"
#include "engine.h"

Magnet::Magnet(Level& level,
			   const Vec2i& position) : Object(level, 1)
{
	renderLayers = RL_MAIN;
	warpTo(position);
	flags = OF_MASSIVE | OF_FIXED | OF_TRANSPORTABLE;
}

Magnet::~Magnet()
{
}

void Magnet::updateSprites()
{
	sprites.add(Vec2i(224, 96));
}

void Magnet::onRender(RenderLayer layer,
					  const Vec4d& color)
{
	if(layer == RL_MAIN) Engine::inst().renderSprites(sprites, color);
}

void Magnet::onUpdate()
{
}

void Magnet::onTouchedByPlayer(Player* p_player)
{
	flash();

	level.turnArrows();
}

void Magnet::onCollision(Object* p_obj)
{
	if(p_obj->getFlags() & OF_ACTIVATOR)
	{
		p_obj->flash();
		onTouchedByPlayer(0);
	}
}