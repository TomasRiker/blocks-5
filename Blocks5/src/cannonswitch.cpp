#include "pch.h"
#include "cannonswitch.h"
#include "barrage.h"
#include "engine.h"

CannonSwitch::CannonSwitch(Level& level,
						   const Vec2i& position,
						   int subType,
						   uint color) : Object(level, 1)
{
	warpTo(position);
	flags = OF_MASSIVE | OF_FIXED;
	this->subType = subType;
	this->color = color;
}

CannonSwitch::~CannonSwitch()
{
}

void CannonSwitch::onRender(int layer,
							const Vec4d& color)
{
	if(layer == 1)
	{
		// Schalter rendern
		Vec2i positionOnTexture(160 + subType * 32, 288);
		Engine::inst().renderSprite(Vec2i(0, 0), positionOnTexture, Vec2i(16, 16), getStdColor(this->color) * color);
	}
}

void CannonSwitch::onUpdate()
{
}

void CannonSwitch::onTouchedByPlayer(Player* p_player)
{
	if(subType == 0)
	{
		// Kanonen abfeuern
		level.fireCannons(color);
	}
	else
	{
		// Kanonen drehen
		level.rotateCannons(color);
	}
}

void CannonSwitch::onCollision(Object* p_obj)
{
	if(p_obj->getFlags() & OF_ACTIVATOR) onTouchedByPlayer(0);
}

bool CannonSwitch::changeInEditor(int mod)
{
	if(!mod)
	{
		color++;
		color %= 6;
	}
	else
	{
		subType++;
		subType %= 2;
	}

	return true;
}

void CannonSwitch::saveAttributes(TiXmlElement* p_target)
{
	p_target->SetAttribute("subType", subType);
	p_target->SetAttribute("color", color);
}

std::string CannonSwitch::getToolTip() const
{
	if(subType == 0) return "$TT_CANNON_FIRE_TRIGGER";
	else return "$TT_CANNON_ROTATE_TRIGGER";
}