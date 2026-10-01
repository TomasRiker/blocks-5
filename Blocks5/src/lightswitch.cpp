#include "pch.h"
#include "lightswitch.h"
#include "engine.h"

LightSwitch::LightSwitch(Level& level,
						 const Vec2i& position) : Object(level, 1)
{
	renderLayers = RL_MAIN | RL_LIGHT;
	warpTo(position);
	flags = OF_MASSIVE | OF_FIXED;
}

LightSwitch::~LightSwitch()
{
}

void LightSwitch::updateSprites()
{
	// switch
	sprites.add(Vec2i(level.isNightVision() ? 192 : 224, 224));
}

void LightSwitch::onRender(RenderLayer layer,
						   const Vec4f& color)
{
	if(layer == RL_MAIN) Engine::inst().renderSprites(sprites, color);
	else if(layer == RL_LIGHT)
	{
		level.renderShine(0.35f, 0.25f + 0.05f * glowJitter);
	}
}

void LightSwitch::onUpdate()
{
}

void LightSwitch::onTouchedByPlayer(Player* p_player)
{
	flash();

	bool nv = !level.isNightVision();
	level.changeNightVision(nv);
	Engine::inst().playSound(nv ? "light_off.ogg" : "light_on.ogg", false, 0.0f, 100);
}

void LightSwitch::onCollision(Object* p_obj)
{
	if(p_obj->getFlags() & OF_ACTIVATOR)
	{
		p_obj->flash();
		onTouchedByPlayer(0);
	}
}