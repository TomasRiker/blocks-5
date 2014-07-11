#include "pch.h"
#include "lightpanel.h"
#include "engine.h"
#include "cf_all.h"

LightPanel::LightPanel(Level& level,
					   const Vec2i& position,
					   int subType) : Panel(level, position)
{
	this->subType = subType;
}

LightPanel::~LightPanel()
{
}

void LightPanel::onRender(int layer,
						  const Vec4d& color)
{
	if(layer == 0)
	{
		// Schalter rendern
		Vec2i positionOnTexture(subType ? 64 : 32, 256);
		Engine::inst().renderSprite(Vec2i(0, 0), positionOnTexture, Vec2i(16, 16), color);
	}
	else if(layer == 18)
	{
		level.renderShine(0.35, 0.25 + random(-0.05, 0.05));
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
		Engine::inst().playSound("light_off.ogg", false, 0.0, 100);
		Engine::inst().crossfade(new CF_ColorBlend(Vec3d(0.0, 0.0, 0.0), 0.1), 1.4);
	}
	else if(subType == 1 && nv)
	{
		level.setNightVision(false);
		Engine::inst().playSound("light_on.ogg", false, 0.0, 100);
		Engine::inst().crossfade(new CF_ColorBlend(Vec3d(1.0, 1.0, 1.0), 0.1), 1.4);
	}
}