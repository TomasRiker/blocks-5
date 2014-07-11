#include "pch.h"
#include "e_pulseswitch.h"
#include "engine.h"

E_PulseSwitch::E_PulseSwitch(Level& level,
							 const Vec2i& position,
							 int pulseValue,
							 int dir) : Electronics(level, position, dir)
{
	this->pulseValue = pulseValue;
	value = !pulseValue;

	// Ausgang erzeugen
	createPin(10, Vec2i(8, 15), PT_OUTPUT);
}

E_PulseSwitch::~E_PulseSwitch()
{
}

void E_PulseSwitch::onRender(int layer,
							 const Vec4d& color)
{
	Electronics::onRender(layer, color);

	if(layer == 1)
	{
		Vec2i t(96 + ((32 * pulseValue) + (value == pulseValue ? 32 : 0)) % 64, 704);
		Engine::inst().renderSprite(Vec2i(0, 0), t, Vec2i(16, 16), color, false, 90.0 * dir);
	}
}

void E_PulseSwitch::saveAttributes(TiXmlElement* p_target)
{
	Electronics::saveAttributes(p_target);

	p_target->SetAttribute("pulseValue", pulseValue);
}

void E_PulseSwitch::saveExtendedAttributes(TiXmlElement* p_target)
{
	Electronics::saveExtendedAttributes(p_target);

	p_target->SetAttribute("value", value);
}

void E_PulseSwitch::loadExtendedAttributes(TiXmlElement* p_element)
{
	Electronics::loadExtendedAttributes(p_element);

	p_element->Attribute("value", &value);
}

std::string E_PulseSwitch::getToolTip() const
{
	if(pulseValue) return "$TT_PULSE_TRIGGER_ONE";
	else return "$TT_PULSE_TRIGGER_ZERO";
}

bool E_PulseSwitch::changeInEditor(int mod)
{
	if(!mod)
	{
		dir++;
		dir %= 4;
	}
	else
	{
		pulseValue++;
		pulseValue %= 2;
		value = !pulseValue;
	}

	return true;
}

void E_PulseSwitch::onTouchedByPlayer(Player* p_player)
{
	value = pulseValue;

	Engine::inst().playSound(pulseValue ? "e_valueswitch_on.ogg" : "e_valueswitch_off.ogg");
}

void E_PulseSwitch::onCollision(Object* p_obj)
{
	if(p_obj->getFlags() & OF_ACTIVATOR) onTouchedByPlayer(0);
}

void E_PulseSwitch::doLogic()
{
	setValue(10, value);
	value = !pulseValue;
}