#include "pch.h"
#include "e_hexdigit.h"
#include "engine.h"

E_HexDigit::E_HexDigit(Level& level,
					   const Vec2i& position,
					   int dir) : Electronics(level, position, dir)
{
	value = -1;

	// Eingänge erzeugen
	createPin(0, Vec2i(0, 13), PT_INPUT);
	createPin(1, Vec2i(0, 9), PT_INPUT);
	createPin(2, Vec2i(0, 6), PT_INPUT);
	createPin(3, Vec2i(0, 2), PT_INPUT);
}

E_HexDigit::~E_HexDigit()
{
}

void E_HexDigit::onRender(int layer,
						  const Vec4d& color)
{
	Electronics::onRender(layer, color);

	if(layer == 1)
	{
		Engine& engine = Engine::inst();
		engine.renderSprite(Vec2i(0, 0), Vec2i(192, 608), Vec2i(16, 16), color, false, 90.0 * dir);
		if(value != -1)
		{
			Vec2i t(0, 640);
			t.x += 32 * (value % 8);
			t.y += 32 * (value / 8);
			engine.renderSprite(Vec2i(0, 0), t, Vec2i(16, 16), color, false, 90.0 * dir);
		}
	}
	else if(layer == 18)
	{
		level.renderShine(0.5, 0.5 + random(-0.05, 0.05));
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
		value = getValue(0) + 2 * getValue(1) + 4 * getValue(2) + 8 * getValue(3);
	}
}