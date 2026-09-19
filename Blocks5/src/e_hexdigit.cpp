#include "pch.h"
#include "e_hexdigit.h"
#include "engine.h"

E_HexDigit::E_HexDigit(Level& level,
					   const Vec2i& position,
					   int dir) : Electronics(level, position, dir)
{
	renderLayers |= RL_MAIN | RL_LIGHT;
	value = -1;

	// create the inputs
	createPin(0, Vec2i(0, 13), PT_INPUT);
	createPin(1, Vec2i(0, 9), PT_INPUT);
	createPin(2, Vec2i(0, 6), PT_INPUT);
	createPin(3, Vec2i(0, 2), PT_INPUT);
}

E_HexDigit::~E_HexDigit()
{
}

void E_HexDigit::updateSprites()
{
	Electronics::updateSprites();
	sprites.add(Vec2i(192, 608)).rotation = 90.0f * dir;
	if(value != -1)
	{
		sprites.add(Vec2i(32 * (value % 8), 640 + 32 * (value / 8))).rotation = 90.0f * dir;
	}
}

void E_HexDigit::onRender(RenderLayer layer,
						  const Vec4f& color)
{
	Electronics::onRender(layer, color);
	if(layer == RL_MAIN) Engine::inst().renderSprites(sprites, color);
	else if(layer == RL_LIGHT)
	{
		level.renderShine(0.5f, 0.5f + 0.05f * glowJitter);
	}
}

bool E_HexDigit::changeInEditor(int mod)
{
	if(!mod)
	{
		dir++;
		dir %= 4;
	}

	return true;
}

void E_HexDigit::doLogic()
{
	if(isAnyInputUndefined()) value = -1;
	else
	{
		// Each input is read as a logic level rather than as its number, which
		// is the coercion the gates' &&, || and ! already make of theirs. A
		// Value or a PulseSwitch carries 0 to 7, and taken as numbers four of
		// those add up to 105 - which as a sprite index is 32*(105%8),
		// 640+32*(105/8), four hundred rows off the bottom of the sheet. Read
		// as levels the sum is 0 to 15 and cannot be anything else, which is
		// what the sixteen digits of the picture are.
		value = (getValue(0) ? 1 : 0)
			  + (getValue(1) ? 2 : 0)
			  + (getValue(2) ? 4 : 0)
			  + (getValue(3) ? 8 : 0);
	}
}