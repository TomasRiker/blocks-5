#include "pch.h"
#include "elevator.h"
#include "rail.h"
#include "soundinstance.h"
#include "engine.h"

uint Elevator::numInstances = 0;
SoundInstance* Elevator::p_soundInst = 0;
bool Elevator::soundChanged = false;

Elevator::Elevator(Level& level,
				   const Vec2i& position,
				   int dir) : Object(level, 300)
{
	warpTo(position);
	flags = OF_FIXED | OF_ELEVATOR;
	this->dir = dir;
	this->interpolation = 0.12;
	moveCounter = 0;
	newDir = dir;
	origDir = dir;
	blink = false;

	if(!level.isInEditor())
	{
		numInstances++;

		if(numInstances == 1)
		{
			// Das ist die erste Instanz. Soundinstanz erzeugen und pausieren.
			Sound* p_sound = Manager<Sound>::inst().request("elevator.ogg");
			p_soundInst = p_sound->createInstance();
			p_sound->release();

			p_soundInst->setVolume(0.0);
			p_soundInst->setPitch(0.1);
			p_soundInst->play(true);
			p_soundInst->pause();
		}
	}
}

Elevator::~Elevator()
{
}

void Elevator::onRemove()
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

void Elevator::onRender(int layer,
						const Vec4d& color)
{
	if(layer == 1)
	{
		// Aufzug rendern
		int frame = level.isInEditor() ? dir : newDir;
		if(blink && (((moveCounter - 1) / 10) % 2)) frame = 4;
		if(!level.isInEditor() && !level.isElectricityOn()) frame = 4;
		Engine::inst().renderSprite(Vec2i(0, 0), Vec2i(frame * 32, 352), Vec2i(16, 16), color);
	}
}

void Elevator::onUpdate()
{
	soundChanged = false;

	if(!level.isElectricityOn()) return;

	if(moveCounter)
	{
		moveCounter--;
		return;
	}

	moveCounter = 15;
	origDir = dir;
	dir = newDir;
	blink = false;

	Vec2i dirV;
	switch(dir)
	{
	case 0: dirV = Vec2i(0, -1); break;
	case 1: dirV = Vec2i(1, 0); break;
	case 2: dirV = Vec2i(0, 1); break;
	case 3: dirV = Vec2i(-1, 0); break;
	}

	// Was für eine Schiene liegt hier?
	Rail* p_thisRail = level.getRailAt(position);
	if(!p_thisRail) return;

	// Was für eine Schiene liegt da?
	Rail* p_nextRail = level.getRailAt(position + dirV);

	bool movementAllowed = false;

	// Gar keine Schiene?
	if(!p_nextRail)
	{
		// Das geht nur, wenn die aktuelle Schiene kaputt ist.
		if(p_thisRail->subType == 4) movementAllowed = true;
	}
	else
	{
		int nextSubType = p_nextRail->subType;
		int nextDir = p_nextRail->dir;

		// gerade Strecke
		if(nextSubType == 0 && (nextDir % 2) == (dir % 2)) movementAllowed = true;

		// Kurve
		else if(nextSubType == 1 && (nextDir == dir || nextDir == ((dir + 1) % 4)))
		{
			if(nextDir == dir) newDir = (dir + 1) % 4;
			else newDir = (dir + 3) % 4;
			movementAllowed = true;
		}

		// Kreuzung
		else if(nextSubType == 2) movementAllowed = true;

		// T-Stück
		else if(nextSubType == 3 && nextDir != (dir + 2) % 4) movementAllowed = true;

		// kaputte Strecke
		else if(nextSubType == 4 && (nextDir % 2) == (dir % 2)) movementAllowed = true;

		// Abzweigung links
		else if(nextSubType == 5)
		{
			if((nextDir % 2) == (dir % 2)) movementAllowed = true;
			else if((nextDir + 1) % 4 == dir)
			{
				movementAllowed = true;
				newDir = (nextDir + 2) % 4;
			}
		}

		// Abzweigung rechts
		else if(nextSubType == 6)
		{
			if((nextDir % 2) == (dir % 2)) movementAllowed = true;
			else if((nextDir + 3) % 4 == dir)
			{
				movementAllowed = true;
				newDir = (nextDir + 2) % 4;
			}
		}
	}

	bool moved = false;
	if(movementAllowed)
	{
		// Ist dort schon ein anderer Aufzug?
		Elevator* p_other = level.getElevatorAt(position + dirV);
		if(!p_other)
		{
			// versuchen, den Aufzug zu bewegen
			moved = move(dirV);
		}
		else
		{
			if(p_other->newDir == newDir)
			{
				// den anderen Aufzug zuerst bewegen
				p_other->onUpdate();
				p_other->moveCounter++;

				if(p_other->position != position + dirV)
				{
					// versuchen, den Aufzug zu bewegen
					moved = move(dirV);
				}
			}
		}
	}

	if(!moved)
	{
		// umkehren
		moveCounter = 60;
		newDir = (origDir + 2) % 4;
		blink = true;
	}
}

void Elevator::onElectricitySwitch(bool on)
{
	if(soundChanged) return;

	// Sound kontrollieren
	if(on)
	{
		p_soundInst->resume();
		p_soundInst->slideVolume(0.4, 0.1);
		p_soundInst->slidePitch(1.0, 0.1);
	}
	else
	{
		p_soundInst->slideVolume(-1.0, 0.05);
		p_soundInst->slidePitch(0.1, 0.05);
	}

	soundChanged = true;
}

bool Elevator::changeInEditor(int mod)
{
	dir++;
	dir %= 4;

	return true;
}

void Elevator::saveAttributes(TiXmlElement* p_target)
{
	p_target->SetAttribute("dir", dir);
}

void Elevator::saveExtendedAttributes(TiXmlElement* p_target)
{
	Object::saveExtendedAttributes(p_target);

	p_target->SetAttribute("moveCounter", moveCounter);
	p_target->SetAttribute("newDir", newDir);
	p_target->SetAttribute("origDir", origDir);
	p_target->SetAttribute("blink", blink ? 1 : 0);
}

void Elevator::loadExtendedAttributes(TiXmlElement* p_element)
{
	Object::loadExtendedAttributes(p_element);

	p_element->Attribute("moveCounter", &moveCounter);
	p_element->Attribute("newDir", &newDir);
	p_element->Attribute("origDir", &origDir);
	int blink;
	p_element->Attribute("blink", &blink);
	this->blink = blink ? true : false;
}