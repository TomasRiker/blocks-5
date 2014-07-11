#include "pch.h"
#include "e_lightbulb.h"
#include "engine.h"

E_LightBulb::E_LightBulb(Level& level,
						 const Vec2i& position,
						 int dir) : Electronics(level, position, dir)
{
	renderBox = false;
	on = false;

	// Eingang erzeugen
	createPin(0, Vec2i(7, 15), PT_INPUT);
}

E_LightBulb::~E_LightBulb()
{
}

void E_LightBulb::onRender(int layer,
						   const Vec4d& color)
{
	Electronics::onRender(layer, color);

	if(layer == 1)
	{
		Engine::inst().renderSprite(Vec2i(0, 0), Vec2i(on ? 160 : 128, 608), Vec2i(16, 16), color, false, 90.0 * dir);
	}
	else if(layer == 18 && on)
	{
		level.renderShine(1.0, 1.5 + random(-0.05, 0.05));
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