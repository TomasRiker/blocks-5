#include "pch.h"
#include "e_valueswitch.h"
#include "engine.h"

E_ValueSwitch::E_ValueSwitch(Level& level,
							 const Vec2i& position,
							 int value,
							 int dir) : Electronics(level, position, dir)
{
	this->value = value;

	// Ausgang erzeugen
	createPin(10, Vec2i(8, 15), PT_OUTPUT);
}

E_ValueSwitch::~E_ValueSwitch()
{
}

void E_ValueSwitch::onRender(int layer,
							 const Vec4d& color)
{
	Electronics::onRender(layer, color);

	if(layer == 1)
	{
		Engine::inst().renderSprite(Vec2i(0, 0), Vec2i(96 + 32 * value, 576), Vec2i(16, 16), color, false, 90.0 * dir);
	}
}

void E_ValueSwitch::saveAttributes(TiXmlElement* p_target)
{
	Electronics::saveAttributes(p_target);

	p_target->SetAttribute("value", value);
}

bool E_ValueSwitch::changeInEditor(int mod)
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

void E_ValueSwitch::onTouchedByPlayer(Player* p_player)
{
	value = !value;

	Engine::inst().playSound(value ? "e_valueswitch_on.ogg" : "e_valueswitch_off.ogg");
}

void E_ValueSwitch::onCollision(Object* p_obj)
{
	if(p_obj->getFlags() & OF_ACTIVATOR) onTouchedByPlayer(0);
}

void E_ValueSwitch::doLogic()
{
	setValue(10, value);
}