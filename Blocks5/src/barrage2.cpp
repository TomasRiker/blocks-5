#include "pch.h"
#include "barrage2.h"
#include "engine.h"

Barrage2::Barrage2(Level& level,
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

Barrage2::~Barrage2()
{
}

void Barrage2::onRender(int layer,
						const Vec4d& color)
{
	if(layer == 1)
	{
		// Blockade rendern
		Vec2i positionOnTexture;
		if(shownState == 5) positionOnTexture = Vec2i(96, 256);
		else if(shownState > 0) positionOnTexture = Vec2i(128, 256);
		else positionOnTexture = Vec2i(160, 256);
		Engine::inst().renderSprite(Vec2i(0, 0), positionOnTexture, Vec2i(16, 16), getStdColor(this->color) * color);
	}
}

void Barrage2::onUpdate()
{
	if(up) shownState++;
	else shownState--;
	shownState = clamp(shownState, 0, 5);

	if(!shownState) ghost = true;
}

bool Barrage2::changeInEditor(int mod)
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

void Barrage2::saveAttributes(TiXmlElement* p_target)
{
	p_target->SetAttribute("up", up ? 1 : 0);
	p_target->SetAttribute("color", color);
}

int Barrage2::change(bool up)
{
	if(this->up == up) return 0;

	// Wenn gerade ein Objekt da ist, dann geht es nicht!
	Object* p_obj = level.getFrontObjectAt(position);
	if(p_obj)
	{
		if(p_obj->getType() != "Rail" && p_obj->getType() != "Barrage2" && ((p_obj->getFlags() & OF_MASSIVE) || p_obj->getType() == "Elevator")) return -1;
	}

	this->up = up;
	updateProperties();

	return 1;
}

uint Barrage2::getColor() const
{
	return color;
}

void Barrage2::updateProperties()
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