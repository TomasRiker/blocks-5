#include "pch.h"
#include "activatorblock.h"
#include "engine.h"

ActivatorBlock::ActivatorBlock(Level& level,
							   const Vec2i& position,
							   bool shielded) : Object(level, 1)
{
	warpTo(position);
	flags = OF_MASSIVE | OF_GRAVITY | (shielded ? 0 : OF_DESTROYABLE) | OF_ACTIVATOR | OF_TRANSPORTABLE | OF_CONVERTABLE | OF_BLOCK_GAS;
	interpolation = 0.3;
	destroyTime = 1;
	this->shielded = shielded;
}

ActivatorBlock::~ActivatorBlock()
{
}

void ActivatorBlock::updateSprites()
{
	// Block
	sprites.add(shielded ? Vec2i(64, 288) : Vec2i(0, 0));
}

void ActivatorBlock::onRender(int layer,
							  const Vec4d& color)
{
	if(layer == 1) Engine::inst().renderSprites(sprites, color);
}

void ActivatorBlock::saveAttributes(TiXmlElement* p_target)
{
	p_target->SetAttribute("shielded", shielded ? 1 : 0);
}

std::string ActivatorBlock::getToolTip() const
{
	if(shielded) return "\xA7" "de:Gepanzerter Aktivator-Block\n(Gravitation; aktiviert Schalter)\xA7" "en:Armored activator block\n(gravity; triggers switches)";
	else return "\xA7" "de:Aktivator-Block\n(Gravitation; aktiviert Schalter)\xA7" "en:Activator block\n(gravity; triggers switches)";
}