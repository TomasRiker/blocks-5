#include "pch.h"
#include "presets.h"
#include "engine.h"
#include "texture.h"
#include "level.h"
#include "debriscolordb.h"
#include "object.h"
#include "stdobject.h"
#include "conveyorbelt.h"
#include "teleporter.h"
#include "arrow.h"
#include "electricityswitch.h"
#include "magnet.h"
#include "diamondmachine.h"
#include "bomb.h"
#include "laser.h"
#include "mirror.h"
#include "barrage.h"
#include "barrageswitch.h"
#include "activatorblock.h"
#include "lightswitch.h"
#include "lightpanel.h"
#include "barrage2.h"
#include "barrage2panel.h"
#include "electricitypanel.h"
#include "player.h"
#include "exit.h"
#include "hint.h"
#include "fire.h"
#include "elevator.h"
#include "rail.h"
#include "enemy.h"
#include "cannon.h"
#include "cannonswitch.h"
#include "cannonpanel.h"
#include "toxicwaste.h"
#include "toxicgas.h"
#include "damage.h"
#include "projectile.h"
#include "hotel.h"
#include "eye.h"
#include "lava.h"
#include "e_value.h"
#include "e_valueswitch.h"
#include "e_pulseswitch.h"
#include "e_pulsepanel.h"
#include "e_clock.h"
#include "e_lightbulb.h"
#include "e_hexdigit.h"
#include "e_barrage.h"
#include "e_gate.h"
#include "e_flipflop.h"
#include "lightbarriersender.h"
#include "e_lightbarrierreceiver.h"
#include "e_multiplexer.h"
#include "e_blockdetector.h"

Presets::Presets(Level& level,
				 Texture* p_sprites) : level(level)
{
	this->p_sprites = p_sprites;
	p_sprites->addRef();

	texCoords["Player"] = Vec2i(0, 224);
	texCoords["Exit"] = Vec2i(224, 32);
	texCoords["Block"] = Vec2i(160, 0);
	texCoords["Block2"] = Vec2i(32, 0);
	texCoords["ShieldedBlock"] = Vec2i(224, 0);
	texCoords["Box"] = Vec2i(128, 0);
	texCoords["ShieldedBox"] = Vec2i(192, 0);
	texCoords["Grass"] = Vec2i(64, 0);
	texCoords["Diamond"] = Vec2i(0, 96);
	texCoords["ConveyorBelt"] = Vec2i(0, 32);
	texCoords["Arrow"] = Vec2i(96, 0);
	texCoords["Teleporter"] = Vec2i(0, 64);
	texCoords["TeleporterNoPlayer"] = Vec2i(192, 96);
	texCoords["ElectricitySwitch"] = Vec2i(128, 96);
	texCoords["Magnet"] = Vec2i(224, 96);
	texCoords["DiamondMachine"] = Vec2i(0, 128);
	texCoords["Bomb"] = Vec2i(0, 160);
	texCoords["Laser"] = Vec2i(0, 192);
	texCoords["Mirror"] = Vec2i(160, 160);
	texCoords["Barrage"] = Vec2i(64, 192);
	texCoords["BarrageSwitch"] = Vec2i(160, 192);
	texCoords["ActivatorBlock"] = Vec2i(0, 0);
	texCoords["ShieldedActivatorBlock"] = Vec2i(64, 288);
	texCoords["LightSwitch"] = Vec2i(192, 224);
	texCoords["Spike"] = Vec2i(192, 160);
	texCoords["Amboss"] = Vec2i(0, 256);
	texCoords["LightPanel"] = Vec2i(32, 256);
	texCoords["Barrage2"] = Vec2i(96, 256);
	texCoords["Barrage2Panel"] = Vec2i(192, 256);
	texCoords["ElectricityPanel"] = Vec2i(0, 288);
	texCoords["Hint"] = Vec2i(96, 288);
	texCoords["Fire"] = Vec2i(0, 320);
	texCoords["IceBox"] = Vec2i(128, 288);
	texCoords["Elevator"] = Vec2i(0, 352);
	texCoords["Rail"] = Vec2i(0, 384);
	texCoords["Enemy"] = Vec2i(0, 416);
	texCoords["Cannon"] = Vec2i(96, 416);
	texCoords["CannonSwitch"] = Vec2i(160, 288);
	texCoords["CannonPanel"] = Vec2i(224, 288);
	texCoords["ToxicWaste"] = Vec2i(192, 352);
	texCoords["Mask"] = Vec2i(224, 352);
	texCoords["Syringe"] = Vec2i(224, 384);
	texCoords["Block3"] = Vec2i(0, 448);
	texCoords["Hotel"] = Vec2i(32, 448);
	texCoords["Eye"] = Vec2i(128, 448);
	texCoords["Lava"] = Vec2i(0, 480);
	texCoords["BlockZero"] = Vec2i(160, 576);
	texCoords["BlockOne"] = Vec2i(192, 576);

	for(stdext::hash_map<std::string, Vec2i>::const_iterator i = texCoords.begin(); i != texCoords.end(); ++i) presetNames.push_back(i->first);
}

Presets::~Presets()
{
	p_sprites->release();
}

void Presets::renderPreset(const std::string& name,
						   const Vec2i& position)
{
	Engine& engine = Engine::inst();

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glTranslated(position.x, position.y, 0.0);

	Vec2i t = texCoords[name];
	bool mirrorX = t.x < 0;
	if(mirrorX) t.x = -t.x;
	engine.renderSprite(p_sprites, Vec2i(0, 0), t, Vec2i(16, 16), Vec4d(1.0, 1.0, 1.0, 1.0), mirrorX);

	glPopMatrix();
}

Object* Presets::instancePreset(const std::string& name,
								const Vec2i& position,
								TiXmlElement* p_element,
								bool fromEditor)
{
	Object* p_theObject = 0;

	stdext::hash_map<std::string, Vec2i>::const_iterator it = texCoords.find(name);
	Vec2i t(0, 0);
	if(it != texCoords.end()) t = it->second;

	std::string newName = name;

	if(name == "Player")
	{
		int character = 0;
		int active = 1;
		if(p_element)
		{
			p_element->QueryIntAttribute("character", &character);
			p_element->QueryIntAttribute("active", &active);
		}

		p_theObject = new Player(level, position, static_cast<uint>(character), active ? true : false);
	}
	else if(name == "Exit")
	{
		p_theObject = new Exit(level, position);
		p_theObject->setToolTip("$TT_EXIT");
	}
	else if(name == "Block")
	{
		StdObject* p_obj = new StdObject(level, position, Object::OF_MASSIVE | Object::OF_GRAVITY | Object::OF_DESTROYABLE | Object::OF_TRANSPORTABLE | Object::OF_CONVERTABLE | Object::OF_BLOCK_GAS, 1, t);
		p_obj->setCollisionSound("block.ogg");
		p_obj->setDestroyTime(100);
		p_theObject = p_obj;
		p_theObject->setToolTip("$TT_BLOCK");
	}
	else if(name == "Block2")
	{
		StdObject* p_obj = new StdObject(level, position, Object::OF_MASSIVE | Object::OF_GRAVITY | Object::OF_DESTROYABLE | Object::OF_TRANSPORTABLE | Object::OF_CONVERTABLE, 1, t);
		p_obj->setCollisionSound("block.ogg");
		p_obj->setDestroyTime(100);
		p_theObject = p_obj;
		p_theObject->setToolTip("$TT_BLOCK");
	}
	else if(name == "Block3")
	{
		StdObject* p_obj = new StdObject(level, position, Object::OF_MASSIVE | Object::OF_GRAVITY | Object::OF_DESTROYABLE | Object::OF_TRANSPORTABLE | Object::OF_CONVERTABLE, 1, t);
		p_obj->setCollisionSound("block.ogg");
		p_obj->setDestroyTime(100);
		p_theObject = p_obj;
		p_theObject->setToolTip("$TT_BLOCK");
	}
	else if(name == "ShieldedBlock")
	{
		StdObject* p_obj = new StdObject(level, position, Object::OF_MASSIVE | Object::OF_GRAVITY | Object::OF_TRANSPORTABLE | Object::OF_CONVERTABLE | Object::OF_BLOCK_GAS, 1, t);
		p_obj->setCollisionSound("block.ogg");
		p_theObject = p_obj;
		p_theObject->setToolTip("$TT_ARMORED_BLOCK");
	}
	else if(name == "Box")
	{
		StdObject* p_obj = new StdObject(level, position, Object::OF_MASSIVE | Object::OF_DESTROYABLE | Object::OF_TRANSPORTABLE | Object::OF_BLOCK_GAS, 1, t);
		p_obj->setDestroyTime(50);
		p_theObject = p_obj;
		p_theObject->setToolTip("$TT_BOX");
	}
	else if(name == "ShieldedBox")
	{
		p_theObject = new StdObject(level, position, Object::OF_MASSIVE | Object::OF_TRANSPORTABLE | Object::OF_BLOCK_GAS, 1, t);
		p_theObject->setToolTip("$TT_ARMORED_BOX");
	}
	else if(name == "Grass")
	{
		StdObject* p_obj = new StdObject(level, position, Object::OF_MASSIVE | Object::OF_COLLECTABLE | Object::OF_FIXED | Object::OF_DESTROYABLE, 2, t);
		p_obj->setCollectData("grass.ogg", ~0);
		p_obj->setDestroyTime(25);
		p_theObject = p_obj;
		p_theObject->setToolTip("$TT_GRASS");
	}
	else if(name == "Diamond")
	{
		StdObject* p_obj = new StdObject(level, position, Object::OF_MASSIVE | Object::OF_COLLECTABLE | Object::OF_DESTROYABLE | Object::OF_TRANSPORTABLE, 1, t);
		p_obj->setAnimation(4, 5);
		p_obj->setCollectData("diamond.ogg", 1);
		p_obj->setGlow(true);
		p_obj->setDestroyTime(200);
		p_theObject = p_obj;
		p_theObject->setToolTip("$TT_DIAMOND");
	}
	else if(name == "ConveyorBelt")
	{
		int dir = 1;
		if(p_element) p_element->QueryIntAttribute("dir", &dir);
		ConveyorBelt* p_obj = new ConveyorBelt(level, position, dir);
		p_theObject = p_obj;
		p_theObject->setToolTip("$TT_CONVEYOR_BELT");
	}
	else if(name == "Arrow")
	{
		int dir = 0;
		if(p_element) p_element->QueryIntAttribute("dir", &dir);
		Arrow* p_obj = new Arrow(level, position, dir);
		p_theObject = p_obj;
		p_theObject->setToolTip("$TT_ARROW");
	}
	else if(name == "Teleporter")
	{
		Vec2i target(1, 1);
		int subType = 0;
		if(p_element)
		{
			p_element->QueryIntAttribute("targetX", &target.x);
			p_element->QueryIntAttribute("targetY", &target.y);
			p_element->QueryIntAttribute("subType", &subType);
		}

		p_theObject = new Teleporter(level, position, target, subType);
	}
	else if(name == "TeleporterNoPlayer")
	{
		p_theObject = new Teleporter(level, position, Vec2i(1, 1), 1);
		newName = "Teleporter";
	}
	else if(name == "ElectricitySwitch")
	{
		p_theObject = new ElectricitySwitch(level, position);
		p_theObject->setToolTip("$TT_ELECTRICITY_SWITCH");
	}
	else if(name == "Magnet")
	{
		p_theObject = new Magnet(level, position);
		p_theObject->setToolTip("$TT_MAGNET");
	}
	else if(name == "DiamondMachine")
	{
		p_theObject = new DiamondMachine(level, position);
		p_theObject->setToolTip("$TT_DIAMOND_MACHINE");
	}
	else if(name == "Bomb")
	{
		p_theObject = new Bomb(level, position);
		p_theObject->setToolTip("$TT_BOMB");
	}
	else if(name == "Laser")
	{
		int dir = 0;
		if(p_element) p_element->QueryIntAttribute("dir", &dir);
		p_theObject = new Laser(level, position, dir);
		p_theObject->setToolTip("$TT_LASER");
	}
	else if(name == "Mirror")
	{
		int subType = 0;
		int dir = 0;
		if(p_element)
		{
			p_element->QueryIntAttribute("subType", &subType);
			p_element->QueryIntAttribute("dir", &dir);
		}

		p_theObject = new Mirror(level, position, subType, dir);
	}
	else if(name == "Barrage")
	{
		int up = 1;
		int color = 0;
		if(p_element)
		{
			p_element->QueryIntAttribute("up", &up);
			p_element->QueryIntAttribute("color", &color);
		}

		p_theObject = new Barrage(level, position, up ? true : false, color);
		p_theObject->setToolTip("$TT_BARRAGE_A");
	}
	else if(name == "BarrageSwitch")
	{
		int color = 0;
		if(p_element) p_element->QueryIntAttribute("color", &color);
		p_theObject = new BarrageSwitch(level, position, color);
		p_theObject->setToolTip("$TT_BARRAGE_A_SWITCH");
	}
	else if(name == "ActivatorBlock")
	{
		int shielded = 0;
		if(p_element) p_element->QueryIntAttribute("shielded", &shielded);
		p_theObject = new ActivatorBlock(level, position, shielded ? true : false);
		p_theObject->setCollisionSound("block.ogg");
		p_theObject->setToolTip("$TT_ACTIVATOR_BLOCK");
	}
	else if(name == "ShieldedActivatorBlock")
	{
		p_theObject = new ActivatorBlock(level, position, true);
		p_theObject->setCollisionSound("block.ogg");
		newName = "ActivatorBlock";
		p_theObject->setToolTip("$TT_ARMORED_ACTIVATOR_BLOCK");
	}
	else if(name == "LightSwitch")
	{
		p_theObject = new LightSwitch(level, position);
		p_theObject->setToolTip("$TT_LIGHT_SWITCH");
	}
	else if(name == "Spike")
	{
		StdObject* p_obj = new StdObject(level, position, Object::OF_MASSIVE | Object::OF_GRAVITY | Object::OF_DESTROYABLE | Object::OF_DEADLY | Object::OF_TRANSPORTABLE, 1, t);
		p_obj->setCollisionSound("block.ogg");
		p_obj->setDestroyTime(100);
		p_obj->setMass(10);
		p_theObject = p_obj;
		p_theObject->setToolTip("$TT_SPIKE");
	}
	else if(name == "Amboss")
	{
		StdObject* p_obj = new StdObject(level, position, Object::OF_MASSIVE | Object::OF_GRAVITY | Object::OF_DESTROYABLE | Object::OF_DEADLY_WEIGHT | Object::OF_TRANSPORTABLE, 1, t);
		p_obj->setCollisionSound("block.ogg");
		p_obj->setDestroyTime(120);
		p_obj->setMass(1000);
		p_theObject = p_obj;
		p_theObject->setToolTip("$TT_AMBOS");
	}
	else if(name == "LightPanel")
	{
		int subType = 0;
		if(p_element) p_element->QueryIntAttribute("subType", &subType);
		p_theObject = new LightPanel(level, position, subType);
		p_theObject->setToolTip("$TT_LIGHT_PANEL");
	}
	else if(name == "Barrage2")
	{
		int up = 1;
		int color = 0;
		if(p_element)
		{
			p_element->QueryIntAttribute("up", &up);
			p_element->QueryIntAttribute("color", &color);
		}

		p_theObject = new Barrage2(level, position, up ? true : false, color);
		p_theObject->setToolTip("$TT_BARRAGE_B");
	}
	else if(name == "Barrage2Panel")
	{
		int subType = fromEditor ? 0 : 1;
		int color = 0;
		if(p_element)
		{
			p_element->QueryIntAttribute("subType", &subType);
			p_element->QueryIntAttribute("color", &color);
		}

		p_theObject = new Barrage2Panel(level, position, subType, color);
		p_theObject->setToolTip("$TT_BARRAGE_B_PANEL");
	}
	else if(name == "ElectricityPanel")
	{
		int subType = 0;
		if(p_element) p_element->QueryIntAttribute("subType", &subType);
		p_theObject = new ElectricityPanel(level, position, subType);
		p_theObject->setToolTip("$TT_ELECTRICITY_PANEL");
	}
	else if(name == "Hint")
	{
		std::string text = loadString("$LE_DEFAULT_HINT_TEXT");
		if(p_element)
		{
			TiXmlElement* p_text = p_element->FirstChildElement("Text");
			const char* p_textChr = p_text->GetText();
			if(p_textChr) text = p_textChr;
		}

		p_theObject = new Hint(level, position, text);
		p_theObject->setToolTip("$TT_HINT");
	}
	else if(name == "Fire")
	{
		p_theObject = new Fire(level, position);
		p_theObject->setToolTip("$TT_FIRE");
	}
	else if(name == "IceBox")
	{
		StdObject* p_obj = new StdObject(level, position, Object::OF_MASSIVE | Object::OF_DESTROYABLE | Object::OF_KILL_FIRE | Object::OF_TRANSPORTABLE | Object::OF_BLOCK_GAS, 1, t);
		p_obj->setDestroyTime(40);
		p_theObject = p_obj;
		p_theObject->setToolTip("$TT_ICE_BOX");
	}
	else if(name == "Elevator")
	{
		int dir = 0;
		if(p_element) p_element->QueryIntAttribute("dir", &dir);
		p_theObject = new Elevator(level, position, dir);
		p_theObject->setToolTip("$TT_PLATFORM");
	}
	else if(name == "Rail")
	{
		int subType = 0;
		int dir = 0;
		if(p_element)
		{
			p_element->QueryIntAttribute("subType", &subType);
			p_element->QueryIntAttribute("dir", &dir);
		}

		p_theObject = new Rail(level, position, subType, dir);
	}
	else if(name == "Enemy")
	{
		int subType = 0;
		int dir = 0;
		if(p_element)
		{
			p_element->QueryIntAttribute("subType", &subType);
			p_element->QueryIntAttribute("dir", &dir);
		}

		p_theObject = new Enemy(level, position, subType, dir);
	}
	else if(name == "Cannon")
	{
		int color = 0;
		int dir = 0;
		if(p_element)
		{
			p_element->QueryIntAttribute("color", &color);
			p_element->QueryIntAttribute("dir", &dir);
		}

		p_theObject = new Cannon(level, position, color, dir);
		p_theObject->setToolTip("$TT_CANNON");
	}
	else if(name == "CannonSwitch")
	{
		int subType = 0;
		int color = 0;
		if(p_element)
		{
			p_element->QueryIntAttribute("subType", &subType);
			p_element->QueryIntAttribute("color", &color);
		}

		p_theObject = new CannonSwitch(level, position, subType, color);
	}
	else if(name == "CannonPanel")
	{
		int color = 0;
		if(p_element) p_element->QueryIntAttribute("color", &color);
		p_theObject = new CannonPanel(level, position, color);
		p_theObject->setToolTip("$TT_CANNON_FIRE_PANEL");
	}
	else if(name == "ToxicWaste")
	{
		p_theObject = new ToxicWaste(level, position);
		p_theObject->setToolTip("$TT_TOXIC_WASTE");
	}
	else if(name == "Mask")
	{
		StdObject* p_obj = new StdObject(level, position, Object::OF_MASSIVE | Object::OF_COLLECTABLE | Object::OF_DESTROYABLE | Object::OF_TRANSPORTABLE, 1, t);
		p_obj->setCollectData("", 2);
		p_obj->setDestroyTime(1);
		p_theObject = p_obj;
		p_theObject->setToolTip("$TT_MASK");
	}
	else if(name == "Syringe")
	{
		StdObject* p_obj = new StdObject(level, position, Object::OF_MASSIVE | Object::OF_COLLECTABLE | Object::OF_DESTROYABLE | Object::OF_TRANSPORTABLE, 1, t);
		p_obj->setCollectData("syringe.ogg", 3);
		p_obj->setDestroyTime(1);
		p_theObject = p_obj;
		p_theObject->setToolTip("$TT_SYRINGE");
	}
	else if(name == "Hotel")
	{
		p_theObject = new Hotel(level, position);
		p_theObject->setToolTip("$TT_HOTEL");
	}
	else if(name == "Eye")
	{
		int dir = -1;
		if(p_element) p_element->QueryIntAttribute("dir", &dir);
		p_theObject = new Eye(level, position, dir);
		p_theObject->setToolTip("$TT_EYE");
	}
	else if(name == "Lava")
	{
		int dir = 0;
		if(p_element) p_element->QueryIntAttribute("dir", &dir);
		p_theObject = new Lava(level, position, dir);
		p_theObject->setToolTip("$TT_LAVA");
	}
	else if(name == "BlockZero")
	{
		StdObject* p_obj = new StdObject(level, position, Object::OF_MASSIVE | Object::OF_GRAVITY | Object::OF_DESTROYABLE | Object::OF_TRANSPORTABLE | Object::OF_CONVERTABLE, 1, t);
		p_obj->setCollisionSound("block.ogg");
		p_obj->setDestroyTime(50);
		p_theObject = p_obj;
		p_theObject->setToolTip("$TT_BLOCK_ZERO");
	}
	else if(name == "BlockOne")
	{
		StdObject* p_obj = new StdObject(level, position, Object::OF_MASSIVE | Object::OF_GRAVITY | Object::OF_DESTROYABLE | Object::OF_TRANSPORTABLE | Object::OF_CONVERTABLE, 1, t);
		p_obj->setCollisionSound("block.ogg");
		p_obj->setDestroyTime(50);
		p_theObject = p_obj;
		p_theObject->setToolTip("$TT_BLOCK_ONE");
	}
	else if(name == "E_Value")
	{
		int dir = 0;
		int value = 0;
		if(p_element)
		{
			p_element->QueryIntAttribute("dir", &dir);
			p_element->QueryIntAttribute("value", &value);
			p_theObject = new E_Value(level, position, value, dir);
		}
	}
	else if(name == "E_ValueSwitch")
	{
		int dir = 0;
		int value = 0;
		if(p_element)
		{
			p_element->QueryIntAttribute("dir", &dir);
			p_element->QueryIntAttribute("value", &value);
			p_theObject = new E_ValueSwitch(level, position, value, dir);
			p_theObject->setToolTip("$TT_VALUE_SWITCH");
		}
	}
	else if(name == "E_PulseSwitch")
	{
		int dir = 0;
		int pulseValue = 0;
		if(p_element)
		{
			p_element->QueryIntAttribute("dir", &dir);
			p_element->QueryIntAttribute("pulseValue", &pulseValue);
			p_theObject = new E_PulseSwitch(level, position, pulseValue, dir);
		}
	}
	else if(name == "E_PulsePanel")
	{
		int dir = 0;
		int pulseValue = 0;
		if(p_element)
		{
			p_element->QueryIntAttribute("dir", &dir);
			p_element->QueryIntAttribute("pulseValue", &pulseValue);
			p_theObject = new E_PulsePanel(level, position, pulseValue, dir);
		}
	}
	else if(name == "E_Clock")
	{
		int dir = 0;
		if(p_element) p_element->QueryIntAttribute("dir", &dir);
		p_theObject = new E_Clock(level, position, dir);
		p_theObject->setToolTip("$TT_CLOCK");
	}
	else if(name == "E_LightBulb")
	{
		int dir = 0;
		if(p_element) p_element->QueryIntAttribute("dir", &dir);
		p_theObject = new E_LightBulb(level, position, dir);
		p_theObject->setToolTip("$TT_LIGHT_BULB");
	}
	else if(name == "E_HexDigit")
	{
		int dir = 0;
		if(p_element) p_element->QueryIntAttribute("dir", &dir);
		p_theObject = new E_HexDigit(level, position, dir);
		p_theObject->setToolTip("$TT_HEX_DIGIT_DISPLAY");
	}
	else if(name == "E_Barrage")
	{
		int dir = 0;
		if(p_element) p_element->QueryIntAttribute("dir", &dir);
		p_theObject = new E_Barrage(level, position, dir);
		p_theObject->setToolTip("$TT_ELECTRONIC_BARRAGE");
	}
	else if(name == "E_Gate")
	{
		int dir = 0;
		int subType = 0;
		if(p_element)
		{
			p_element->QueryIntAttribute("dir", &dir);
			p_element->QueryIntAttribute("subType", &subType);
			p_theObject = new E_Gate(level, position, subType, dir);
		}
	}
	else if(name == "E_FlipFlop")
	{
		int dir = 0;
		int subType = 0;
		int value = 0;
		if(p_element)
		{
			p_element->QueryIntAttribute("dir", &dir);
			p_element->QueryIntAttribute("subType", &subType);
			p_element->QueryIntAttribute("value", &value);
			p_theObject = new E_FlipFlop(level, position, subType, value, dir);
		}
	}
	else if(name == "LightBarrierSender")
	{
		int dir = 0;
		if(p_element) p_element->QueryIntAttribute("dir", &dir);
		p_theObject = new LightBarrierSender(level, position, dir);
		p_theObject->setToolTip("$TT_LIGHT_BARRIER_SENDER");
	}
	else if(name == "E_LightBarrierReceiver")
	{
		int dir = 0;
		if(p_element) p_element->QueryIntAttribute("dir", &dir);
		p_theObject = new E_LightBarrierReceiver(level, position, dir);
		p_theObject->setToolTip("$TT_LIGHT_BARRIER_RECEIVER");
	}
	else if(name == "E_Multiplexer")
	{
		int dir = 0;
		if(p_element) p_element->QueryIntAttribute("dir", &dir);
		p_theObject = new E_Multiplexer(level, position, dir);
		p_theObject->setToolTip("$TT_MULTIPLEXER");
	}
	else if(name == "E_BlockDetector")
	{
		int dir = 0;
		if(p_element) p_element->QueryIntAttribute("dir", &dir);
		p_theObject = new E_BlockDetector(level, position, dir);
		p_theObject->setToolTip("$TT_BLOCK_DETECTOR");
	}

	// Objekttypen, die immer nur in Zwischenspeicherungen vorkommen
	else if(name == "Damage")
	{
		double rotation = -1.0;
		p_element->QueryDoubleAttribute("rotation", &rotation);
		p_theObject = new Damage(level, position, rotation);
	}
	else if(name == "ToxicGas")
	{
		p_theObject = new ToxicGas(level, position);
	}
	else if(name == "Projectile")
	{
		p_theObject = new Projectile(level, position * 16, Vec2d(0.0));
	}

	if(p_theObject)
	{
		// Typ eintragen
		p_theObject->setType(newName);

		if(p_theObject->positionOnTexture.x == -1)
		{
			// Texturkoordinaten setzen
			p_theObject->positionOnTexture = t;
		}

		if(p_theObject->getFlags() & Object::OF_DESTROYABLE ||
		   p_theObject->getFlags() & Object::OF_CONVERTABLE)
		{
			// Trümmerfarbe berechnen
			p_theObject->setDebrisColor(DebrisColorDB::inst().getDebrisColor(p_sprites, p_theObject->positionOnTexture));
		}
	}

	return p_theObject;
}

const std::vector<std::string>& Presets::getPresetNames() const
{
	return presetNames;
}