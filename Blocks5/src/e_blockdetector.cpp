#include "pch.h"
#include "e_blockdetector.h"
#include "engine.h"

E_BlockDetector::E_BlockDetector(Level& level,
								 const Vec2i& position,
								 int dir) : Electronics(level, position, dir)
{
	// Ausgang erzeugen
	createPin(10, Vec2i(7, 15), PT_OUTPUT);
}

E_BlockDetector::~E_BlockDetector()
{
}

void E_BlockDetector::updateSprites()
{
	Electronics::updateSprites();
	sprites.add(Vec2i(224, 576)).rotation = 90.0 * dir;
}

void E_BlockDetector::onRender(int layer,
							   const Vec4d& color)
{
	Electronics::onRender(layer, color);
	if(layer == 1) Engine::inst().renderSprites(sprites, color);
}

bool E_BlockDetector::changeInEditor(int mod)
{
	if(!mod)
	{
		dir++;
		dir %= 4;
	}

	return true;
}

void E_BlockDetector::doLogic()
{
	setValue(10, -1);

	// Ist da ein Eins- oder Null-Block?
	Vec2i p = position + intToDir(dir);
	Object* p_obj = level.getFrontObjectAt(p);
	if(p_obj)
	{
		if(p_obj->getType() == "BlockZero") setValue(10, 0);
		else if(p_obj->getType() == "BlockOne") setValue(10, 1);
	}
}