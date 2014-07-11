#include "pch.h"
#include "e_multiplexer.h"
#include "engine.h"

E_Multiplexer::E_Multiplexer(Level& level,
							 const Vec2i& position,
							 int dir) : Electronics(level, position, dir)
{
	value = -1;

	// Eingänge erzeugen
	createPin(0, Vec2i(0, 2), PT_INPUT);
	createPin(1, Vec2i(0, 12), PT_INPUT);
	createPin(2, Vec2i(8, 0), PT_INPUT);

	// Ausgang erzeugen
	createPin(10, Vec2i(15, 7), PT_OUTPUT);
}

E_Multiplexer::~E_Multiplexer()
{
}

void E_Multiplexer::onRender(int layer,
							 const Vec4d& color)
{
	Electronics::onRender(layer, color);

	if(layer == 1)
	{
		Vec2i t(value == -1 ? 0 : (value == 0 ? 32 : 64), 576);
		Engine::inst().renderSprite(Vec2i(0, 0), t, Vec2i(16, 16), color, false, 90.0 * dir);
	}
}

void E_Multiplexer::saveExtendedAttributes(TiXmlElement* p_target)
{
	Electronics::saveExtendedAttributes(p_target);

	p_target->SetAttribute("value", value);
}

void E_Multiplexer::loadExtendedAttributes(TiXmlElement* p_element)
{
	Electronics::loadExtendedAttributes(p_element);

	p_element->Attribute("value", &value);
}

bool E_Multiplexer::changeInEditor(int mod)
{
	if(!mod)
	{
		dir++;
		dir %= 4;
	}

	return true;
}

void E_Multiplexer::doLogic()
{
	value = getValue(2);
	if(value == 0) setValue(10, getValue(0));
	else if(value == 1) setValue(10, getValue(1));
	else setValue(10, -1);
}