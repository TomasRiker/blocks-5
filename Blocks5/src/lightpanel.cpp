#include "pch.h"
#include "lightpanel.h"
#include "engine.h"
#include "cf_all.h"

LightPanel::LightPanel(Level& level,
					   const Vec2i& position,
					   int subType) : Panel(level, position)
{
	renderLayers = RL_FLOOR | RL_LIGHT;
	this->subType = subType;
}

LightPanel::~LightPanel()
{
}

void LightPanel::updateSprites()
{
	// switch
	sprites.add(Vec2i(subType ? 64 : 32, 256));
}

void LightPanel::onRender(RenderLayer layer,
						  const Vec4f& color)
{
	if(layer == RL_FLOOR) Engine::inst().renderSprites(sprites, color);
	else if(layer == RL_LIGHT)
	{
		level.renderShine(0.35f, 0.25f + 0.05f * glowJitter);
	}
}

bool LightPanel::changeInEditor(int mod)
{
	subType++;
	subType %= 2;

	return true;
}

void LightPanel::saveAttributes(TiXmlElement* p_target)
{
	p_target->SetAttribute("subType", subType);
}

void LightPanel::onTriggered(Object* p_sender)
{
	bool nv = level.isNightVision();
	if(subType == 0 && !nv)
	{
		level.setNightVision(true);
		Engine::inst().playSound("light_off.ogg", false, 0.0f, 100);
		Engine::inst().crossfade(new CF_ColorBlend(Vec3f(0.0f, 0.0f, 0.0f), 0.1f), 1.4f);
	}
	else if(subType == 1 && nv)
	{
		level.setNightVision(false);
		Engine::inst().playSound("light_on.ogg", false, 0.0f, 100);
		Engine::inst().crossfade(new CF_ColorBlend(Vec3f(1.0f, 1.0f, 1.0f), 0.1f), 1.4f);
	}
}