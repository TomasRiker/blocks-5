#include "pch.h"
#include "lightswitch.h"
#include "engine.h"
#include "cf_all.h"

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
	level.setNightVision(nv);

	if(nv)
	{
		Engine::inst().playSound("light_off.ogg", false, 0.0f, 100);
		Engine::inst().crossfade(new CF_ColorBlend(Vec3f(0.0f, 0.0f, 0.0f), 0.1f), 1.4f);
	}
	else
	{
		Engine::inst().playSound("light_on.ogg", false, 0.0f, 100);
		Engine::inst().crossfade(new CF_ColorBlend(Vec3f(1.0f, 1.0f, 1.0f), 0.1f), 1.4f);
	}
}

void LightSwitch::onCollision(Object* p_obj)
{
	if(p_obj->getFlags() & OF_ACTIVATOR)
	{
		p_obj->flash();
		onTouchedByPlayer(0);
	}
}