#include "pch.h"
#include "E_FlipFlop.h"
#include "engine.h"

E_FlipFlop::E_FlipFlop(Level& level,
					   const Vec2i& position,
					   int subType,
					   int value,
					   int dir) : Electronics(level, position, dir)
{
	this->subType = subType;
	this->value = value;

	// Pins erzeugen
	switch(subType)
	{
	case 0:
	case 1:
		createPin(0, Vec2i(0, 3), PT_INPUT);
		createPin(1, Vec2i(0, 13), PT_INPUT);
		createPin(10, Vec2i(15, 8), PT_OUTPUT);
		break;

	case 2:
		createPin(0, Vec2i(0, 0), PT_INPUT);
		createPin(1, Vec2i(0, 8), PT_INPUT);
		createPin(2, Vec2i(0, 15), PT_INPUT);
		createPin(10, Vec2i(15, 8), PT_OUTPUT);
		break;
	}
}

E_FlipFlop::~E_FlipFlop()
{
}

void E_FlipFlop::onRender(int layer,
						  const Vec4d& color)
{
	Electronics::onRender(layer, color);

	if(layer == 1)
	{
		Vec2i t(64 * subType + 32 * value, 544);
		Engine::inst().renderSprite(Vec2i(0, 0), t, Vec2i(16, 16), color, false, 90.0 * dir);
	}
}

void E_FlipFlop::saveAttributes(TiXmlElement* p_target)
{
	Electronics::saveAttributes(p_target);

	p_target->SetAttribute("subType", subType);
	p_target->SetAttribute("value", value);
}

std::string E_FlipFlop::getToolTip() const
{
	char* p_str[] = {"$TT_FLIP_FLOP_RS",
					 "$TT_FLIP_FLOP_D",
					 "$TT_FLIP_FLOP_JK"};

	return p_str[subType];
}

bool E_FlipFlop::changeInEditor(int mod)
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

void E_FlipFlop::doLogic()
{
	switch(subType)
	{
	case 0:
		{
			// RS-Flipflop, ungetaktet
			if(isAnyInputUndefined()) break;
			int s = getValue(0);
			int r = getValue(1);
			if(s && !r) value = 1;
			else if(r && !s) value = 0;
			break;
		}

	case 1:
		{
			// D-Flipflop, flankengesteuert
			if(isAnyInputUndefined()) break;
			int d = getValue(0);
			int clk = getValue(1);
			int oldClk = getOldValue(1);
			if(clk && !oldClk) value = d;
			break;
		}

	case 2:
		{
			// JK-Flipflop, flankengesteuert
			if(isAnyInputUndefined()) break;
			int j = getValue(0);
			int clk = getValue(1);
			int oldClk = getOldValue(1);
			int k = getValue(2);
			if(clk && !oldClk)
			{
				if(!j && k) value = 0;
				else if(j && !k) value = 1;
				else if(j && k) value = !value;
			}
		}
	}

	setValue(10, value);
}