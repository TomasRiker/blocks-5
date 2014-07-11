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
	debrisColor = Vec4d(1.0, 0.5, 0.0, 0.25);
	this->shielded = shielded;
	anim = 0;
}

ActivatorBlock::~ActivatorBlock()
{
}

void ActivatorBlock::onRender(int layer,
							  const Vec4d& color)
{
	if(layer == 1)
	{
		// Block rendern
		Vec4d realColor(color);
		if(anim)
		{
			double a = (1.0 / 20.0) * anim;
			double t = level.time * 0.1;
			realColor.r *= 0.75 + 0.25 * cos(a * t);
			realColor.g *= 0.75 + 0.25 * cos(1.2 * a * t);
			realColor.b *= 0.75 + 0.25 * cos(1.4 * a * t);
			realColor.a *= 0.75 + 0.25 * cos(1.6 * a * t);
		}

		Engine::inst().renderSprite(Vec2i(0, 0), shielded ? Vec2i(64, 288) : Vec2i(0, 0), Vec2i(16, 16), realColor);
	}
}

void ActivatorBlock::onUpdate()
{
	if(anim) anim--;
}

void ActivatorBlock::onCollision(Object* p_obj)
{
	// Animation starten
	anim = 20;
}

void ActivatorBlock::saveAttributes(TiXmlElement* p_target)
{
	p_target->SetAttribute("shielded", shielded ? 1 : 0);
}

std::string ActivatorBlock::getToolTip() const
{
	if(shielded) return "§de:Gepanzerter Aktivator-Block\n(Gravitation; aktiviert Schalter)§en:Armored activator block\n(gravity; triggers switches)";
	else return "§de:Aktivator-Block\n(Gravitation; aktiviert Schalter)§en:Activator block\n(gravity; triggers switches)";
}