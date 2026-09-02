#include "pch.h"
#include "magnet.h"
#include "engine.h"

Magnet::Magnet(Level& level,
			   const Vec2i& position) : Object(level, 1)
{
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

void Magnet::onRender(int layer,
					  const Vec4d& color)
{
	if(layer == 1) Engine::inst().renderSprites(sprites, color);
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
	if(p_obj->getFlags() & OF_ACTIVATOR) onTouchedByPlayer(0);
}