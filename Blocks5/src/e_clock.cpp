#include "pch.h"
#include "e_clock.h"
#include "engine.h"

E_Clock::E_Clock(Level& level,
				 const Vec2i& position,
				 int dir) : Electronics(level, position, dir)
{
	value = 0;

	// Ausgang erzeugen
	createPin(10, Vec2i(15, 8), PT_OUTPUT);
}

E_Clock::~E_Clock()
{
}

void E_Clock::onRender(int layer,
					   const Vec4d& color)
{
	Electronics::onRender(layer, color);

	if(layer == 1)
	{
		Engine::inst().renderSprite(Vec2i(0, 0), Vec2i(192, 544), Vec2i(16, 16), color, false, 90.0 * dir);
	}
}

void E_Clock::saveExtendedAttributes(TiXmlElement* p_target)
{
	Electronics::saveExtendedAttributes(p_target);

	p_target->SetAttribute("value", value);
}

void E_Clock::loadExtendedAttributes(TiXmlElement* p_element)
{
	Electronics::loadExtendedAttributes(p_element);

	p_element->Attribute("value", &value);
}

bool E_Clock::changeInEditor(int mod)
{
	if(!mod)
	{
		dir++;
		dir %= 4;
	}

	return true;
}

void E_Clock::doLogic()
{
	setValue(10, value < 5 ? 0 : 1);
	value++;
	value %= 10;
}