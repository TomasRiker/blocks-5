#include "pch.h"
#include "electricitypanel.h"
#include "engine.h"
#include "cf_all.h"

ElectricityPanel::ElectricityPanel(Level& level,
								   const Vec2i& position,
								   int subType) : Panel(level, position)
{
	renderLayers = RL_FLOOR;
	this->subType = subType;
}

ElectricityPanel::~ElectricityPanel()
{
}

void ElectricityPanel::updateSprites()
{
	// switch
	sprites.add(Vec2i(subType ? 32 : 0, 288));
}

void ElectricityPanel::onRender(RenderLayer layer,
								const Vec4d& color)
{
	if(layer == RL_FLOOR) Engine::inst().renderSprites(sprites, color);
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