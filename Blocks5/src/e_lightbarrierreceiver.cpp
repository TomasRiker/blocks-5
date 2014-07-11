#include "pch.h"
#include "e_lightbarrierreceiver.h"
#include "engine.h"

E_LightBarrierReceiver::E_LightBarrierReceiver(Level& level,
											   const Vec2i& position,
											   int dir) : Electronics(level, position, dir)
{
	renderBox = false;
	value = 0;

	// Ausgang erzeugen
	createPin(10, Vec2i(8, 15), PT_OUTPUT);
}

E_LightBarrierReceiver::~E_LightBarrierReceiver()
{
}

void E_LightBarrierReceiver::frameBegin()
{
	Object::frameBegin();
	value = 0;
}

void E_LightBarrierReceiver::onRender(int layer,
									  const Vec4d& color)
{
	Electronics::onRender(layer, color);

	if(layer == 1)
	{
		Engine::inst().renderSprite(Vec2i(0, 0), Vec2i(96, 608), Vec2i(16, 16), color, false, 90.0 * dir);
	}
}

void E_LightBarrierReceiver::saveExtendedAttributes(TiXmlElement* p_target)
{
	Electronics::saveExtendedAttributes(p_target);

	p_target->SetAttribute("value", value);
}

void E_LightBarrierReceiver::loadExtendedAttributes(TiXmlElement* p_element)
{
	Electronics::loadExtendedAttributes(p_element);

	p_element->Attribute("value", &value);
}

bool E_LightBarrierReceiver::changeInEditor(int mod)
{
	if(!mod)
	{
		dir++;
		dir %= 4;
	}

	return true;
}

void E_LightBarrierReceiver::doLogic()
{
	setValue(10, value);
}

bool E_LightBarrierReceiver::reflectLaser(Vec2i& dir,
										  bool lightBarrier)
{
	if(lightBarrier)
	{
		if(dirToInt(dir) == (this->dir + 2) % 4)
		{
			value = 1;
			dir = Vec2i(0, 0);
			return true;
		}
	}

	return false;
}