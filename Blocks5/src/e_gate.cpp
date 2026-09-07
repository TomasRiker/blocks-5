#include "pch.h"
#include "e_gate.h"
#include "engine.h"

E_Gate::E_Gate(Level& level,
			   const Vec2i& position,
			   int subType,
			   int dir) : Electronics(level, position, dir)
{
	// subType comes out of the level file unchecked (presets.cpp), and levels
	// travel between players. Four things downstream index on it: the pin count
	// just below, the sprite region in updateSprites(), the switch in doLogic()
	// and the tooltip table. Catching it once here is what keeps all four safe,
	// and 0 is what getToolTip() has always fallen back to.
	if(subType < 0 || subType > 7)
	{
		printfLog("+ WARNING: Gate with subType %d, which does not exist. Treating it as AND.\n",
				  subType);
		subType = 0;
	}

	this->subType = subType;

	// create the inputs
	if(subType == 6 || subType == 7)
	{
		createPin(0, Vec2i(0, 8), PT_INPUT);
	}
	else
	{
		createPin(0, Vec2i(0, 5), PT_INPUT);
		createPin(1, Vec2i(0, 10), PT_INPUT);
	}

	// create the output
	createPin(10, Vec2i(15, 8), PT_OUTPUT);
}

E_Gate::~E_Gate()
{
}

void E_Gate::updateSprites()
{
	Electronics::updateSprites();
	Vec2i t(32 + 32 * subType, 512);
	if(subType == 7) t = Vec2i(224, 544);
	sprites.add(t).rotation = 90.0 * dir;
}

void E_Gate::onRender(int layer,
					  const Vec4d& color)
{
	Electronics::onRender(layer, color);
	if(layer == 1) Engine::inst().renderSprites(sprites, color);
}

void E_Gate::saveAttributes(TiXmlElement* p_target)
{
	Electronics::saveAttributes(p_target);

	p_target->SetAttribute("subType", subType);
}

std::string E_Gate::getToolTip() const
{
	// const, because a string literal is not a char*: that has not been allowed
	// since C++11 - MSVC merely warned about it, GCC and Clang reject it.
	static const char* const p_str[] = {"$TT_GATE_AND",
										"$TT_GATE_NAND",
										"$TT_GATE_OR",
										"$TT_GATE_NOR",
										"$TT_GATE_XOR",
										"$TT_GATE_XNOR",
										"$TT_GATE_NOT",
										"$TT_GATE_PASS_THROUGH"};

	// The constructor clamps subType into range, so this cannot fire today. It
	// stays as the backstop for a ninth gate type added without extending the
	// table: a wrong tooltip rather than a read past the end.
	if(subType < 0 || subType >= static_cast<int>(sizeof(p_str) / sizeof(p_str[0])))
		return p_str[0];

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
	// Undefined inputs give an undefined output.
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