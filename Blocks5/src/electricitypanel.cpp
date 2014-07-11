#include "pch.h"
#include "electricitypanel.h"
#include "engine.h"
#include "cf_all.h"

ElectricityPanel::ElectricityPanel(Level& level,
								   const Vec2i& position,
								   int subType) : Panel(level, position)
{
	this->subType = subType;
}

ElectricityPanel::~ElectricityPanel()
{
}

void ElectricityPanel::onRender(int layer,
								const Vec4d& color)
{
	Vec2i positionOnTexture(subType ? 32 : 0, 288);
	if(layer == 0)
	{
		// Schalter rendern
		Engine::inst().renderSprite(Vec2i(0, 0), positionOnTexture, Vec2i(16, 16), color);
	}
}

bool ElectricityPanel::changeInEditor(int mod)
{
	subType++;
	subType %= 2;

	return true;
}

void ElectricityPanel::saveAttributes(TiXmlElement* p_target)
{
	p_target->SetAttribute("subType", subType);
}

void ElectricityPanel::onTriggered(Object* p_sender)
{
	bool eo = level.isElectricityOn();
	if(subType == 0 && !eo)
	{
		level.setElectricityOn(true);
		Engine::inst().playSound("electricityswitch.ogg", false, 0.15);
	}
	else if(subType == 1 && eo)
	{
		level.setElectricityOn(false);
		Engine::inst().playSound("electricityswitch.ogg", false, 0.15);
	}
}