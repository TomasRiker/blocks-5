#include "pch.h"
#include "lightswitch.h"
#include "engine.h"
#include "cf_all.h"

LightSwitch::LightSwitch(Level& level,
						 const Vec2i& position) : Object(level, 1)
{
	warpTo(position);
	flags = OF_MASSIVE | OF_FIXED;
}

LightSwitch::~LightSwitch()
{
}

void LightSwitch::onRender(int layer,
						   const Vec4d& color)
{
	if(layer == 1)
	{
		// Schalter rendern
		Vec2i positionOnTexture(level.isNightVision() ? 192 : 224, 224);
		Engine::inst().renderSprite(Vec2i(0, 0), positionOnTexture, Vec2i(16, 16), color);
	}
	else if(layer == 18)
	{
		level.renderShine(0.35, 0.25 + random(-0.05, 0.05));
	}
}

void LightSwitch::onUpdate()
{
}

void LightSwitch::onTouchedByPlayer(Player* p_player)
{
	bool nv = !level.isNightVision();
	level.setNightVision(nv);

	if(nv)
	{
		Engine::inst().playSound("light_off.ogg", false, 0.0, 100);
		Engine::inst().crossfade(new CF_ColorBlend(Vec3d(0.0, 0.0, 0.0), 0.1), 1.4);
	}
	else
	{
		Engine::inst().playSound("light_on.ogg", false, 0.0, 100);
		Engine::inst().crossfade(new CF_ColorBlend(Vec3d(1.0, 1.0, 1.0), 0.1), 1.4);
	}
}

void LightSwitch::onCollision(Object* p_obj)
{
	if(p_obj->getFlags() & OF_ACTIVATOR) onTouchedByPlayer(0);
}