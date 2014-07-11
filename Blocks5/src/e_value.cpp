#include "pch.h"
#include "e_value.h"
#include "engine.h"

E_Value::E_Value(Level& level,
				 const Vec2i& position,
				 int value,
				 int dir) : Electronics(level, position, dir)
{
	this->value = value;

	// Ausgang erzeugen
	createPin(10, Vec2i(8, 15), PT_OUTPUT);
}

E_Value::~E_Value()
{
}

void E_Value::onRender(int layer,
					   const Vec4d& color)
{
	Electronics::onRender(layer, color);

	if(layer == 1)
	{
		Engine::inst().renderSprite(Vec2i(0, 0), Vec2i(32 * value, 608), Vec2i(16, 16), color, false, 90.0 * dir);
	}
}

void E_Value::saveAttributes(TiXmlElement* p_target)
{
	Electronics::saveAttributes(p_target);

	p_target->SetAttribute("value", value);
}

std::string E_Value::getToolTip() const
{
	return value ? "$TT_VALUE_ONE" : "$TT_VALUE_ZERO";
}

bool E_Value::changeInEditor(int mod)
{
	if(!mod)
	{
		dir++;
		dir %= 4;
	}
	else
	{
		value++;
		value %= 2;
	}

	return true;
}

void E_Value::doLogic()
{
	setValue(10, value);
}