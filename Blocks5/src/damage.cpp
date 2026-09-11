#include "pch.h"
#include "damage.h"
#include "engine.h"

Damage::Damage(Level& level,
			   const Vec2i& position,
			   double rotation) : Object(level, 400)
{
	renderLayers = RL_FLOOR;
	type = "Damage";
	warpTo(position);
	flags = 0;
	if(rotation == -1.0) this->rotation = random(0.0, 360.0);
	else this->rotation = rotation;
	ghost = true;
}

Damage::~Damage()
{
}

void Damage::updateSprites()
{
	// burnt ground - the only object sprite that is not 16x16
	Sprite& sprite = sprites.add(Vec2i(209, 129));
	sprite.size = Vec2i(46, 46);
	sprite.offset = Vec2i(-16, -16);
	sprite.rotation = rotation;
}

void Damage::onRender(RenderLayer layer,
					  const Vec4d& color)
{
	if(layer == RL_FLOOR) Engine::inst().renderSprites(sprites, color);
}

void Damage::onUpdate()
{
}

void Damage::saveAttributes(TiXmlElement* p_target)
{
	char s[256] = "";
	sprintf(s, "%f", rotation);
	p_target->SetAttribute("rotation", s);
}