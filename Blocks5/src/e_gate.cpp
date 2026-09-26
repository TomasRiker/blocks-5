#include "pch.h"
#include "e_gate.h"
#include "engine.h"

E_Gate::E_Gate(Level& level,
			   const Vec2i& position,
			   int subType,
			   int dir) : Electronics(level, position, dir)
{
	renderLayers |= RL_MAIN;
	// subType comes unchecked out of a level file (presets.cpp), which may be
	// anybody's. The pin count below, the sprite region, doLogic()'s switch
	// and the tooltip table all depend on it, so it is clamped once here; 0
	// is also getToolTip()'s fallback.
	if(subType < 0 || subType > 7)
	{
		printfLog("+ WARNING: Gate with subType %d, which does not exist. Treating it as AND.\n",
				  subType);
		subType = 0;
	}

	this->subType = subType;

	createInputs();
	createPin(10, Vec2i(15, 8), PT_OUTPUT);
}

bool E_Gate::hasOneInput() const
{
	// NOT and pass-through
	return subType == 6 || subType == 7;
}

void E_Gate::createInputs()
{
	// Deleting a pin breaks its connections.
	for(uint i = 0; i < inputPins.size(); i++) delete inputPins[i];
	inputPins.clear();

	if(hasOneInput())
	{
		createPin(0, Vec2i(0, 8), PT_INPUT);
	}
	else
	{
		createPin(0, Vec2i(0, 5), PT_INPUT);
		createPin(1, Vec2i(0, 10), PT_INPUT);
	}
}

E_Gate::~E_Gate()
{
}

void E_Gate::updateSprites()
{
	Electronics::updateSprites();
	Vec2i t(32 + 32 * subType, 512);
	if(subType == 7) t = Vec2i(224, 544);
	sprites.add(t).rotation = 90.0f * dir;
}

void E_Gate::onRender(RenderLayer layer,
					  const Vec4f& color)
{
	Electronics::onRender(layer, color);
	if(layer == RL_MAIN) Engine::inst().renderSprites(sprites, color);
}

void E_Gate::saveAttributes(TiXmlElement* p_target)
{
	Electronics::saveAttributes(p_target);

	p_target->SetAttribute("subType", subType);
}

std::string E_Gate::getToolTip() const
{
	static const char* const p_str[] = {"$TT_GATE_AND",
										"$TT_GATE_NAND",
										"$TT_GATE_OR",
										"$TT_GATE_NOR",
										"$TT_GATE_XOR",
										"$TT_GATE_XNOR",
										"$TT_GATE_NOT",
										"$TT_GATE_PASS_THROUGH"};

	// The constructor clamps subType; this is the backstop for a gate type
	// added without extending the table - a wrong tooltip rather than a read
	// past the end.
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
		// Between a two-input gate and NOT or pass-through the inputs are
		// built anew, since they sit elsewhere. Input 0 is there in both and
		// keeps its wire; input 1's goes, as the next load would drop it.
		const bool oneInput = hasOneInput();
		subType++;
		subType %= 8;
		if(hasOneInput() != oneInput)
		{
			// Copied out first: deleting the pin empties its set.
			const Pin* p_input = getPinByID(0);
			std::set<Pin*> sources;
			if(p_input) sources = p_input->getConnectedPins();

			createInputs();
			for(std::set<Pin*>::const_iterator i = sources.begin(); i != sources.end(); ++i) Pin::connect(*i, getPinByID(0));
		}
	}

	return true;
}

void E_Gate::doLogic()
{
	// An unconnected or undefined input gives an undefined output.
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