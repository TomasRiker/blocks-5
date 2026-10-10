#include "pch.h"
#include "activatorblock.h"
#include "engine.h"

ActivatorBlock::ActivatorBlock(Level& level,
							   const Vec2i& position,
							   bool shielded) : Object(level, 1)
{
	renderLayers = RL_MAIN;
	warpTo(position);
	flags = OF_MASSIVE | OF_GRAVITY | (shielded ? 0 : OF_DESTROYABLE) | OF_ACTIVATOR | OF_TRANSPORTABLE | OF_CONVERTABLE | OF_BLOCK_GAS;
	interpolation = 0.3f;
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

void ActivatorBlock::onRender(RenderLayer layer,
							  const Vec4f& color)
{
	if(layer == RL_MAIN) Engine::inst().renderSprites(sprites, color);
}

void ActivatorBlock::saveAttributes(TiXmlElement* p_target)
{
	p_target->SetAttribute("shielded", shielded ? 1 : 0);
}

std::string ActivatorBlock::getToolTip() const
{
	return shielded ? "$TT_ARMORED_ACTIVATOR_BLOCK" : "$TT_ACTIVATOR_BLOCK";
}