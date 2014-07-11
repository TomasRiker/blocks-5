#include "pch.h"
#include "damage.h"
#include "engine.h"

Damage::Damage(Level& level,
			   const Vec2i& position,
			   double rotation) : Object(level, 400)
{
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

void Damage::onRender(int layer,
					  const Vec4d& color)
{
	if(layer == 0)
	{
		// verbrannten Boden rendern
		Engine::inst().renderSprite(Vec2i(-16, -16), Vec2i(209, 129), Vec2i(46, 46), color, false, rotation);
	}
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