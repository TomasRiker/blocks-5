#include "pch.h"
#include "conveyorbelt.h"
#include "engine.h"
#include "soundinstance.h"

uint ConveyorBelt::numInstances = 0;
SoundInstance* ConveyorBelt::p_soundInst = 0;
bool ConveyorBelt::soundChanged = false;

ConveyorBelt::ConveyorBelt(Level& level,
						   const Vec2i& position,
						   int dir) : Object(level, 1)
{
	warpTo(position);
	flags = OF_MASSIVE | OF_FIXED | OF_BLOCK_GAS;
	this->dir = dir;
	anim = random(0, 6);
	p_objOnMe = 0;
	counter = 0;

	if(!level.isInEditor())
	{
		numInstances++;

		if(numInstances == 1)
		{
			// Das ist die erste Instanz. Soundinstanz erzeugen und pausieren.
			Sound* p_sound = Manager<Sound>::inst().request("conveyorbelt.ogg");
			p_soundInst = p_sound->createInstance();
			p_sound->release();

			if(p_soundInst)
			{
				p_soundInst->setVolume(0.0);
				p_soundInst->setPitch(0.1);
				p_soundInst->play(true);
				p_soundInst->pause();
			}
		}
	}
}

ConveyorBelt::~ConveyorBelt()
{
}

void ConveyorBelt::onRemove()
{
	if(!level.isInEditor())
	{
		numInstances--;

		if(!numInstances)
		{
			// Das war die letzte Instanz. Sound stoppen.
			p_soundInst->stop();
			p_soundInst = 0;
			soundChanged = false;
		}
	}
}

void ConveyorBelt::onRender(int layer,
							const Vec4d& color)
{
	if(layer == 1)
	{
		// Fließband rendern
		Vec2i positionOnTexture((anim / 2 % 7) * 32, 32);
		Engine::inst().renderSprite(Vec2i(0, 0), positionOnTexture, Vec2i(16, 16), color, dir == -1);
	}
}

void ConveyorBelt::onUpdate()
{
	soundChanged = false;

	if(level.isElectricityOn())
	{
		// Befindet sich ein Objekt auf dem Fließband?
		Object* p_obj = level.getFrontObjectAt(position - Vec2i(0, 1));
		if(p_obj)
		{
			if(p_obj->isPushedFromAbove() && !p_obj->isTeleporting())
			{
				if(p_obj == p_objOnMe)
				{
					counter++;
				}
				else
				{
					p_objOnMe = p_obj;
					counter = 0;
				}

				if(counter >= 15)
				{
					// Das Objekt wird verschoben.
					p_obj->onConveyorBelt = 15;
					if(p_obj->move(Vec2i(dir, 0)))
					{
						counter = 0;
					}
				}
			}
			else counter = 0;
		}
		else
		{
			p_objOnMe = 0;
			counter = 0;
		}

		anim++;
	}
}

void ConveyorBelt::onElectricitySwitch(bool on)
{
	if(soundChanged) return;
	if(!p_soundInst) return;

	// Sound kontrollieren
	if(on)
	{
		p_soundInst->resume();
		p_soundInst->slideVolume(0.8, 0.1);
		p_soundInst->slidePitch(1.0, 0.1);
	}
	else
	{
		p_soundInst->slideVolume(-1.0, 0.05);
		p_soundInst->slidePitch(0.1, 0.05);
	}

	soundChanged = true;
}

bool ConveyorBelt::changeInEditor(int mod)
{
	dir = -dir;

	return true;
}

void ConveyorBelt::saveAttributes(TiXmlElement* p_target)
{
	p_target->SetAttribute("dir", dir);
}

int ConveyorBelt::getDir() const
{
	return dir;
}