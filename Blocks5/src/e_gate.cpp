#include "pch.h"
#include "e_gate.h"
#include "engine.h"

E_Gate::E_Gate(Level& level,
			   const Vec2i& position,
			   int subType,
			   int dir) : Electronics(level, position, dir)
{
	this->subType = subType;

	// Eingänge erzeugen
	if(subType == 6 || subType == 7)
	{
		createPin(0, Vec2i(0, 8), PT_INPUT);
	}
	else
	{
		createPin(0, Vec2i(0, 5), PT_INPUT);
		createPin(1, Vec2i(0, 10), PT_INPUT);
	}

	// Ausgang erzeugen
	createPin(10, Vec2i(15, 8), PT_OUTPUT);
}

E_Gate::~E_Gate()
{
}

void E_Gate::onRender(int layer,
					  const Vec4d& color)
{
	Electronics::onRender(layer, color);

	if(layer == 1)
	{
		Vec2i t(32 + 32 * subType, 512);
		if(subType == 7) t = Vec2i(224, 544);
		Engine::inst().renderSprite(Vec2i(0, 0), t, Vec2i(16, 16), color, false, 90.0 * dir);
	}
}

void E_Gate::saveAttributes(TiXmlElement* p_target)
{
	Electronics::saveAttributes(p_target);

	p_target->SetAttribute("subType", subType);
}

std::string E_Gate::getToolTip() const
{
	char* p_str[] = {"$TT_GATE_AND",
					 "$TT_GATE_NAND",
					 "$TT_GATE_OR",
					 "$TT_GATE_NOR",
					 "$TT_GATE_XOR",
					 "$TT_GATE_XNOR",
					 "$TT_GATE_NOT",
					 "$TT_GATE_PASS_THROUGH"};

	return p_str[subType];
}

bool E_Gate::changeInEditor(int mod)
{
	if(!mod)
	{
		dir++;
		dir %= 4;
	}
	else
	{
		subType++;
		subType %= 8;
	}

	return true;
}

void E_Gate::doLogic()
{
	// Undefinierte Eingänge führen zu einem undefinierten Ausgang.
	if(!areAllInputsConnected() || isAnyInputUndefined())
	{
		setAllOutputsToUndefined();
		return;
	}

	if(subType == 6)
	{
		setValue(10, !getValue(0));
	}
	else if(subType == 7)
	{
		setValue(10, getValue(0));
	}
	else
	{
		int x = getValue(0);
		int y = getValue(1);
		int z;

		switch(subType)
		{
		case 0: z = x && y; break;
		case 1: z = !(x && y); break;
		case 2: z = x || y; break;
		case 3: z = !(x || y); break;
		case 4: z = x ^ y; break;
		case 5: z = !(x ^ y); break;
		}

		setValue(10, z);
	}
}