#include "pch.h"
#include "barrage2panel.h"
#include "engine.h"

Barrage2Panel::Barrage2Panel(Level& level,
							 const Vec2i& position,
							 int subType,
							 uint color) : Panel(level, position)
{
	this->subType = subType;
	this->color = color;
}

Barrage2Panel::~Barrage2Panel()
{
}

void Barrage2Panel::onRender(int layer,
							 const Vec4d& color)
{
	if(layer == 0)
	{
		// Schalter rendern
		Vec2i positionOnTexture(subType ? 224 : 192, 256);
		Engine::inst().renderSprite(Vec2i(0, 0), positionOnTexture, Vec2i(16, 16), getStdColor(this->color) * color);
	}
}

bool Barrage2Panel::changeInEditor(int mod)
{
	if(!mod)
	{
		subType++;
		subType %= 2;
	}
	else
	{
		color++;
		color %= 6;
	}

	return true;
}

void Barrage2Panel::saveAttributes(TiXmlElement* p_target)
{
	p_target->SetAttribute("subType", subType);
	p_target->SetAttribute("color", color);
}

void Barrage2Panel::onTriggered(Object* p_sender)
{
	// schalten
	bool up = subType ? false : true;
	level.changeBarrages2(color, up);
}