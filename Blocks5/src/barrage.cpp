#include "pch.h"
#include "barrage.h"
#include "player.h"
#include "engine.h"

Barrage::Barrage(Level& level,
				 const Vec2i& position,
				 bool up,
				 uint color) : Object(level, 0)
{
	warpTo(position);
	this->up = up;
	this->color = color;
	shownState = up ? 5 : 0;
	updateProperties();
}

Barrage::~Barrage()
{
}

void Barrage::onRender(int layer,
					   const Vec4d& color)
{
	if(layer == 1)
	{
		// Blockade rendern
		Vec2i positionOnTexture;
		if(shownState == 5) positionOnTexture = Vec2i(64, 192);
		else if(shownState > 0) positionOnTexture = Vec2i(96, 192);
		else positionOnTexture = Vec2i(128, 192);
		Engine::inst().renderSprite(Vec2i(0, 0), positionOnTexture, Vec2i(16, 16), getStdColor(this->color) * color);
	}
}

void Barrage::onUpdate()
{
	if(up) shownState++;
	else shownState--;
	shownState = clamp(shownState, 0, 5);

	if(!shownState) ghost = true;
}

bool Barrage::changeInEditor(int mod)
{
	if(!mod)
	{
		up = !up;
		shownState = up ? 5 : 0;
	}
	else
	{
		color++;
		color %= 6;
	}

	return true;
}

void Barrage::saveAttributes(TiXmlElement* p_target)
{
	p_target->SetAttribute("up", up ? 1 : 0);
	p_target->SetAttribute("color", color);
}

bool Barrage::change()
{
	// Wenn gerade ein Objekt da ist, dann geht es nicht!
	Object* p_obj = level.getFrontObjectAt(position);
	if(p_obj)
	{
		if(p_obj->getType() != "Rail" && p_obj->getType() != "Barrage" && ((p_obj->getFlags() & OF_MASSIVE) || p_obj->getType() == "Elevator")) return false;
	}

	up = !up;
	updateProperties();

	return true;
}

uint Barrage::getColor() const
{
	return color;
}

void Barrage::updateProperties()
{
	if(up)
	{
		flags = OF_MASSIVE | OF_FIXED | OF_NO_SHADOW;
		ghost = false;
		setDepth(1);
	}
	else
	{
		flags = OF_FIXED | OF_NO_SHADOW;
		setDepth(400);
	}
}