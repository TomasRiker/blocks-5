#include "pch.h"
#include "rail.h"
#include "engine.h"

Rail::Rail(Level& level,
		   const Vec2i& position,
		   int subType,
		   int dir) : Object(level, 301)
{
	warpTo(position);
	flags = OF_FIXED | OF_RAIL;
	this->subType = subType;
	this->dir = dir;
}

Rail::~Rail()
{
}

void Rail::onRender(int layer,
					const Vec4d& color)
{
	if(layer == 1)
	{
		// Aufzug rendern
		Engine::inst().renderSprite(Vec2i(0, 0), Vec2i(subType * 32, 384), Vec2i(16, 16), color, false, 90.0 * dir);
	}
}

void Rail::onUpdate()
{
}

bool Rail::changeInEditor(int mod)
{
	if(!mod)
	{
		dir++;
		dir %= 4;
	}
	else
	{
		subType++;
		subType %= 7;
	}

	return true;
}

void Rail::saveAttributes(TiXmlElement* p_target)
{
	p_target->SetAttribute("subType", subType);
	p_target->SetAttribute("dir", dir);
}

std::string Rail::getToolTip() const
{
	switch(subType % 7)
	{
	case 0: return "$TT_RAILS_STRAIGHT";
	case 1: return "$TT_RAILS_CORNER";
	case 2: return "$TT_RAILS_JUNCTION";
	case 3: return "$TT_RAILS_T_JUNCTION";
	case 4: return "$TT_RAILS_DAMAGED";
	case 5: return "$TT_RAILS_TURN_OFF_LEFT";
	case 6: return "$TT_RAILS_TURN_OFF_RIGHT";
	}

	return toolTip;
}