#include "pch.h"
#include "cannonpanel.h"
#include "engine.h"
#include "barrage.h"

CannonPanel::CannonPanel(Level& level,
						 const Vec2i& position,
						 uint color) : Panel(level, position)
{
	this->color = color;
}

CannonPanel::~CannonPanel()
{
}

void CannonPanel::updateSprites()
{
	// Schalter
	sprites.add(Vec2i(224, 288), getStdColor(this->color));
}

void CannonPanel::onRender(int layer,
						   const Vec4d& color)
{
	if(layer == 0) Engine::inst().renderSprites(sprites, color);
}

bool CannonPanel::changeInEditor(int mod)
{
	color++;
	color %= 6;

	return true;
}

void CannonPanel::saveAttributes(TiXmlElement* p_target)
{
	p_target->SetAttribute("color", color);
}

void CannonPanel::onTriggered(Object* p_sender)
{
	level.fireCannons(color);
}