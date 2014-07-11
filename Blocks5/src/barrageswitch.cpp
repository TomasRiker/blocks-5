#include "pch.h"
#include "barrageswitch.h"
#include "engine.h"

BarrageSwitch::BarrageSwitch(Level& level,
							 const Vec2i& position,
							 uint color) : Object(level, 1)
{
	warpTo(position);
	flags = OF_MASSIVE | OF_FIXED;
	this->color = color;
}

BarrageSwitch::~BarrageSwitch()
{
}

void BarrageSwitch::onRender(int layer,
							 const Vec4d& color)
{
	if(layer == 1)
	{
		// Schalter rendern
		Vec2i positionOnTexture(160, 192);
		Engine::inst().renderSprite(Vec2i(0, 0), positionOnTexture, Vec2i(16, 16), getStdColor(this->color) * color);
	}
}

void BarrageSwitch::onUpdate()
{
}

void BarrageSwitch::onTouchedByPlayer(Player* p_player)
{
	// schalten
	level.changeBarrages(color);
}

void BarrageSwitch::onCollision(Object* p_obj)
{
	if(p_obj->getFlags() & OF_ACTIVATOR) onTouchedByPlayer(0);
}

bool BarrageSwitch::changeInEditor(int mod)
{
	color++;
	color %= 6;

	return true;
}

void BarrageSwitch::saveAttributes(TiXmlElement* p_target)
{
	p_target->SetAttribute("color", color);
}