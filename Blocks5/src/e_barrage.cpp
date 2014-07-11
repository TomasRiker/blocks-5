#include "pch.h"
#include "e_barrage.h"
#include "engine.h"

E_Barrage::E_Barrage(Level& level,
					 const Vec2i& position,
					 int dir) : Electronics(level, position, dir)
{
	renderBox = false;
	up = false;
	shownState = 0;
	updateProperties();

	// Eingang erzeugen
	createPin(0, Vec2i(7, 15), PT_INPUT);
}

E_Barrage::~E_Barrage()
{
}

void E_Barrage::onRender(int layer,
						 const Vec4d& color)
{
	Electronics::onRender(layer, color);

	if(layer == 1)
	{
		// Blockade rendern
		Vec2i positionOnTexture;
		if(shownState == 5) positionOnTexture = Vec2i(0, 704);
		else if(shownState > 0) positionOnTexture = Vec2i(32, 704);
		else positionOnTexture = Vec2i(64, 704);
		Engine::inst().renderSprite(Vec2i(0, 0), positionOnTexture, Vec2i(16, 16), color, false, 90.0 * dir);
	}
}

void E_Barrage::onUpdate()
{
	if(up) shownState++;
	else shownState--;
	shownState = clamp(shownState, 0, 5);

	if(!shownState) ghost = true;
}

void E_Barrage::saveExtendedAttributes(TiXmlElement* p_target)
{
	Electronics::saveExtendedAttributes(p_target);

	p_target->SetAttribute("up", up ? 1 : 0);
	p_target->SetAttribute("shownState", shownState);
}

void E_Barrage::loadExtendedAttributes(TiXmlElement* p_element)
{
	Electronics::loadExtendedAttributes(p_element);

	int up; p_element->Attribute("up", &up);
	this->up = up ? true : false;

	p_element->Attribute("shownState", &shownState);

	updateProperties();
}

bool E_Barrage::changeInEditor(int mod)
{
	if(!mod)
	{
		dir++;
		dir %= 4;
	}

	return true;
}

void E_Barrage::doLogic()
{
	bool targetUp = getValue(0) == 1;

	if(targetUp && !up)
	{
		Object* p_obj = level.getFrontObjectAt(position);
		if(p_obj)
		{
			if(p_obj->getType() != "Rail" &&
			   p_obj->getType() != "Barrage" &&
			   ((p_obj->getFlags() & OF_MASSIVE) || p_obj->getType() == "Elevator"))
			{
				return;
			}
		}
	}

	if(targetUp != up)
	{
		up = targetUp;
		updateProperties();
	}
}

void E_Barrage::updateProperties()
{
	if(up)
	{
		flags = OF_MASSIVE | OF_FIXED | OF_NO_SHADOW | OF_ELECTRONICS;
		ghost = false;
		setDepth(1);
	}
	else
	{
		flags = OF_FIXED | OF_NO_SHADOW | OF_ELECTRONICS;
		setDepth(400);
	}
}