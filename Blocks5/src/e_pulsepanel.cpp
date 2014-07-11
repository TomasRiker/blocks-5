#include "pch.h"
#include "e_pulsepanel.h"
#include "engine.h"

E_PulsePanel::E_PulsePanel(Level& level,
						   const Vec2i& position,
						   int pulseValue,
						   int dir) : Electronics(level, position, dir)
{
	renderBox = false;
	flags &= ~OF_MASSIVE;

	this->pulseValue = pulseValue;
	value = !pulseValue;

	// Ausgang erzeugen
	createPin(10, Vec2i(8, 15), PT_OUTPUT);
}

E_PulsePanel::~E_PulsePanel()
{
}

void E_PulsePanel::onRender(int layer,
							const Vec4d& color)
{
	Electronics::onRender(layer, color);

	if(layer == 0)
	{
		Vec2i t(160 + ((32 * pulseValue) + (value == pulseValue ? 32 : 0)) % 64, 704);
		Engine::inst().renderSprite(Vec2i(0, 0), t, Vec2i(16, 16), color, false, 90.0 * dir);
	}
}

void E_PulsePanel::onUpdate()
{
	// Befindet sich ein Objekt auf dem Panel, das vorher noch nicht da war?
	const std::vector<Object*> newObjectsOnMe = level.getObjectsAt2(position);
	for(std::vector<Object*>::const_iterator i = newObjectsOnMe.begin(); i != newObjectsOnMe.end(); ++i)
	{
		Object* p_obj = *i;
		if(p_obj == this) continue;

		if(std::find(objectsOnMe.begin(), objectsOnMe.end(), p_obj) == objectsOnMe.end())
		{
			if(p_obj->getFlags() & OF_TRIGGER_PANELS)
			{
				// Panel auslösen
				value = pulseValue;
				Engine::inst().playSound(pulseValue ? "e_valueswitch_on.ogg" : "e_valueswitch_off.ogg");
				break;
			}
		}
	}

	objectsOnMe = newObjectsOnMe;
}

void E_PulsePanel::saveAttributes(TiXmlElement* p_target)
{
	Electronics::saveAttributes(p_target);

	p_target->SetAttribute("pulseValue", pulseValue);
}

void E_PulsePanel::saveExtendedAttributes(TiXmlElement* p_target)
{
	Electronics::saveExtendedAttributes(p_target);

	p_target->SetAttribute("value", value);
}

void E_PulsePanel::loadExtendedAttributes(TiXmlElement* p_element)
{
	Electronics::loadExtendedAttributes(p_element);

	p_element->Attribute("value", &value);
}

std::string E_PulsePanel::getToolTip() const
{
	if(pulseValue) return "$TT_PULSE_PANEL_ONE";
	else return "$TT_PULSE_PANEL_ZERO";
}

bool E_PulsePanel::changeInEditor(int mod)
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

void E_PulsePanel::doLogic()
{
	setValue(10, value);
	value = !pulseValue;
}