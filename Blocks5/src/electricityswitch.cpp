#include "pch.h"
#include "electricityswitch.h"
#include "engine.h"

ElectricitySwitch::ElectricitySwitch(Level& level,
									 const Vec2i& position) : Object(level, 1)
{
	warpTo(position);
	flags = OF_MASSIVE | OF_FIXED;
}

ElectricitySwitch::~ElectricitySwitch()
{
}

void ElectricitySwitch::updateSprites()
{
	// Schalter
	sprites.add(Vec2i(level.isElectricityOn() ? 160 : 128, 96));
}

void ElectricitySwitch::onRender(int layer,
								 const Vec4d& color)
{
	if(layer == 1) Engine::inst().renderSprites(sprites, color);
}

void ElectricitySwitch::onUpdate()
{
}

void ElectricitySwitch::onTouchedByPlayer(Player* p_player)
{
	flash();

	level.setElectricityOn(!level.isElectricityOn());
	Engine::inst().playSound("electricityswitch.ogg", false, 0.15, 100);
}

void ElectricitySwitch::onCollision(Object* p_obj)
{
	if(p_obj->getFlags() & OF_ACTIVATOR)
	{
		p_obj->flash();
		onTouchedByPlayer(0);
	}
}