#include "pch.h"
#include "e_lightbulb.h"
#include "engine.h"

E_LightBulb::E_LightBulb(Level& level,
						 const Vec2i& position,
						 int dir) : Electronics(level, position, dir)
{
	renderLayers = RL_MAIN | RL_LIGHT;
	renderBox = false;
	on = false;

	// create the input
	createPin(0, Vec2i(7, 15), PT_INPUT);
}

E_LightBulb::~E_LightBulb()
{
}

void E_LightBulb::updateSprites()
{
	Electronics::updateSprites();
	sprites.add(Vec2i(on ? 160 : 128, 608)).rotation = 90.0 * dir;
}

void E_LightBulb::onRender(RenderLayer layer,
						   const Vec4d& color)
{
	Electronics::onRender(layer, color);
	if(layer == RL_MAIN) Engine::inst().renderSprites(sprites, color);
	else if(layer == RL_LIGHT && on)
	{
		level.renderShine(1.0, 1.5 + 0.05 * glowJitter);
	}
}

bool E_LightBulb::changeInEditor(int mod)
{
	if(!mod)
	{
		dir++;
		dir %= 4;
	}

	return true;
}

void E_LightBulb::doLogic()
{
	on = getValue(0) == 1;
}