#include "pch.h"
#include "electronics.h"
#include "pin.h"
#include "engine.h"
#include "linedrawer.h"

Electronics::Electronics(Level& level,
						 const Vec2i& position,
						 int dir) : Object(level, 1)
{
	warpTo(position);
	flags = OF_FIXED | OF_MASSIVE | OF_ELECTRONICS;
	this->dir = dir;
	renderBox = true;
	positionOnTexture = Vec2i(0, 512);

	if(!level.isInEditor() && !level.isInCat()) level.allElectronics.insert(this);
}

Electronics::~Electronics()
{
}

void Electronics::updateAll(Level& level)
{
	// die Werte an den Ausgängen aller Bauteile an die angeschlossenen Eingänge weiterleiten
	for(std::set<Electronics*>::const_iterator i = level.allElectronics.begin(); i != level.allElectronics.end(); ++i)
	{
		(*i)->propagateOutputs();
	}

	// die Logikfunktionen ausführen
	for(std::set<Electronics*>::const_iterator i = level.allElectronics.begin(); i != level.allElectronics.end(); ++i)
	{
		(*i)->doLogic();
	}
}

void Electronics::onRemove()
{
	// alle Pins löschen
	for(uint i = 0; i < inputPins.size(); i++) delete inputPins[i];
	for(uint i = 0; i < outputPins.size(); i++) delete outputPins[i];
	inputPins.clear();
	outputPins.clear();

	if(!level.isInEditor() && !level.isInCat()) level.allElectronics.erase(this);
}

void Electronics::onRender(int layer,
						   const Vec4d& color)
{
	if(layer == 1 && renderBox)
	{
		// Elektronik-Block rendern
		Engine::inst().renderSprite(Vec2i(0, 0), Vec2i(0, 512), Vec2i(16, 16), color);
	}

	if(layer == 939)
	{
		const Vec4d wireColor[] = {Vec4d(0.35, 0.3, 0.3, 1.0),
								   Vec4d(0.3, 0.35, 0.3, 1.0),
								   Vec4d(0.3, 0.3, 0.35, 1.0),
								   Vec4d(0.35, 0.35, 0.3, 1.0),
								   Vec4d(0.35, 0.3, 0.35, 1.0),
								   Vec4d(0.3, 0.35, 0.35, 1.0),
								   Vec4d(0.35, 0.35, 0.35, 1.0)};

		glDisable(GL_TEXTURE_2D);

		// Verbindungen der Ausgänge rendern
		int n = position.x + position.y;
		for(uint i = 0; i < outputPins.size(); i++)
		{
			const Pin* p_pin1 = outputPins[i];
			const std::set<Pin*>& connectedPins = p_pin1->getConnectedPins();
			for(std::set<Pin*>::const_iterator j = connectedPins.begin(); j != connectedPins.end(); ++j)
			{
				const Pin* p_pin2 = *j;
				LineDrawer line(Pin::getConnectionPath(p_pin1, p_pin2), 1.5f, wireColor[n++ % 7]);
				line.draw();
			}
		}

		glEnable(GL_TEXTURE_2D);
	}
}

void Electronics::saveAttributes(TiXmlElement* p_target)
{
	p_target->SetAttribute("dir", dir);
}

void Electronics::saveExtendedAttributes(TiXmlElement* p_target)
{
	// die aktuellen und ehemaligen Werte aller Pins speichern
	for(std::vector<Pin*>::const_iterator i = inputPins.begin(); i != inputPins.end(); ++i)
	{
		int id = (*i)->getPinID();
		char attrName[256] = "";

		sprintf(attrName, "value%d", id);
		p_target->SetAttribute(attrName, (*i)->getValue());

		sprintf(attrName, "oldValue%d", id);
		p_target->SetAttribute(attrName, (*i)->getOldValue());
	}

	for(std::vector<Pin*>::const_iterator i = outputPins.begin(); i != outputPins.end(); ++i)
	{
		int id = (*i)->getPinID();
		char attrName[256] = "";

		sprintf(attrName, "value%d", id);
		p_target->SetAttribute(attrName, (*i)->getValue());

		sprintf(attrName, "oldValue%d", id);
		p_target->SetAttribute(attrName, (*i)->getOldValue());
	}
}

void Electronics::loadExtendedAttributes(TiXmlElement* p_element)
{
	// die aktuellen und ehemaligen Werte aller Pins laden
	for(std::vector<Pin*>::const_iterator i = inputPins.begin(); i != inputPins.end(); ++i)
	{
		int id = (*i)->getPinID();
		char attrName[256] = "";

		sprintf(attrName, "value%d", id);
		int v = 0x7FFFFFF;
		p_element->QueryIntAttribute(attrName, &v);
		if(v != 0x7FFFFFF) (*i)->writeValue(v);

		sprintf(attrName, "oldValue%d", id);
		v = 0x7FFFFFFF;
		p_element->QueryIntAttribute(attrName, &v);
		if(v != 0x7FFFFFF) (*i)->writeOldValue(v);
	}

	for(std::vector<Pin*>::const_iterator i = outputPins.begin(); i != outputPins.end(); ++i)
	{
		int id = (*i)->getPinID();
		char attrName[256] = "";

		sprintf(attrName, "value%d", id);
		int v = 0x7FFFFFF;
		p_element->QueryIntAttribute(attrName, &v);
		if(v != 0x7FFFFFF) (*i)->writeValue(v);

		sprintf(attrName, "oldValue%d", id);
		v = 0x7FFFFFFF;
		p_element->QueryIntAttribute(attrName, &v);
		if(v != 0x7FFFFFF) (*i)->writeOldValue(v);
	}
}

void Electronics::doLogic()
{
}

void Electronics::saveConnections(TiXmlElement* p_target)
{
	// Verbindungen aller Ausgang-Pins speichern
	TiXmlElement* p_outputs = new TiXmlElement("OutputConnections");
	for(uint i = 0; i < outputPins.size(); i++)
	{
		Pin* p_pin = outputPins[i];
		const std::set<Pin*>& connections = p_pin->getConnectedPins();
		if(!connections.empty())
		{
			for(std::set<Pin*>::const_iterator i = connections.begin(); i != connections.end(); ++i)
			{
				Pin* p_otherPin = *i;
				TiXmlElement* p_connection = new TiXmlElement("Connection");
				p_connection->SetAttribute("sourcePinID", p_pin->getPinID());
				p_connection->SetAttribute("targetX", p_otherPin->getObject()->getPosition().x);
				p_connection->SetAttribute("targetY", p_otherPin->getObject()->getPosition().y);
				p_connection->SetAttribute("targetPinID", p_otherPin->getPinID());
				p_outputs->LinkEndChild(p_connection);
			}
		}
	}

	p_target->LinkEndChild(p_outputs);
}

void Electronics::loadConnections(TiXmlElement* p_source)
{
	TiXmlElement* p_outputs = p_source->FirstChildElement("OutputConnections");
	if(p_outputs)
	{
		TiXmlElement* p_connection = p_outputs->FirstChildElement("Connection");
		while(p_connection)
		{
			int sourcePinID; p_connection->Attribute("sourcePinID", &sourcePinID);
			int targetX; p_connection->Attribute("targetX", &targetX);
			int targetY; p_connection->Attribute("targetY", &targetY);
			int targetPinID; p_connection->Attribute("targetPinID", &targetPinID);
			Pin* p_sourcePin = getPinByID(sourcePinID);

			// Elektronik-Objekt an dieser Stelle suchen
			const std::vector<Object*>& objects = level.getAllObjectsAt(Vec2i(targetX, targetY));
			for(std::vector<Object*>::const_iterator i = objects.begin(); i != objects.end(); ++i)
			{
				if((*i)->getFlags() & OF_ELECTRONICS)
				{
					// die Verbindung erzeugen
					Electronics* p_otherObject = static_cast<Electronics*>(*i);
					Pin::connect(p_sourcePin, p_otherObject->getPinByID(targetPinID));
				}
			}

			p_connection = p_connection->NextSiblingElement("Connection");
		}
	}
}

const std::vector<Pin*>& Electronics::getInputPins() const
{
	return inputPins;
}

const std::vector<Pin*>& Electronics::getOutputPins() const
{
	return outputPins;
}

Pin* Electronics::getPinByID(uint pinID)
{
	for(std::vector<Pin*>::const_iterator i = inputPins.begin(); i != inputPins.end(); ++i) if((*i)->getPinID() == pinID) return *i;
	for(std::vector<Pin*>::const_iterator i = outputPins.begin(); i != outputPins.end(); ++i) if((*i)->getPinID() == pinID) return *i;

	return 0;
}

Pin* Electronics::getPinAt(const Vec2i& where)
{
	Vec2i realPosition = transformFromScreen(where);

	// Welcher Pin ist da?
	for(std::vector<Pin*>::const_iterator i = inputPins.begin(); i != inputPins.end(); ++i)
	{
		int distSq = (realPosition - (*i)->getPosition()).lengthSq();
		if(distSq <= 8) return *i;
	}

	for(std::vector<Pin*>::const_iterator i = outputPins.begin(); i != outputPins.end(); ++i)
	{
		int distSq = (realPosition - (*i)->getPosition()).lengthSq();
		if(distSq <= 8) return *i;
	}

	// Kein Pin an dieser Stelle gefunden!
	return 0;
}

Vec2i Electronics::transformToScreen(const Vec2i& position) const
{
	switch(dir % 4)
	{
	case 0: return position;
	case 1: return Vec2i(15 - position.y, position.x);
	case 2: return Vec2i(15, 15) - position;
	case 3: return Vec2i(position.y, 15 - position.x);
	}

	return Vec2i(-1);
}

Vec2i Electronics::transformFromScreen(const Vec2i& position) const
{
	switch(dir % 4)
	{
	case 0: return position;
	case 1: return Vec2i(position.y, 15 - position.x);
	case 2: return Vec2i(15, 15) - position;
	case 3: return Vec2i(15 - position.y, position.x);
	}

	return Vec2i(-1);
}

void Electronics::createPin(int pinID,
							const Vec2i& position,
							PinType pinType)
{
	Pin* p_pin = new Pin(this, pinID, position, pinType);
	(pinType == PT_INPUT ? inputPins : outputPins).push_back(p_pin);
}

int Electronics::getValue(int pinID)
{
	Pin* p_pin = getPinByID(pinID);
	if(p_pin) return p_pin->getValue();
	else return -1;
}

int Electronics::getOldValue(int pinID)
{
	Pin* p_pin = getPinByID(pinID);
	if(p_pin) return p_pin->getOldValue();
	else return -1;
}

void Electronics::setValue(int pinID,
						   int value)
{
	Pin* p_pin = getPinByID(pinID);
	if(p_pin) p_pin->setValue(value);
}

bool Electronics::areAllInputsConnected() const
{
	for(uint i = 0; i < inputPins.size(); i++)
	{
		if(!inputPins[i]->isConnected()) return false;
	}

	return true;
}

bool Electronics::isAnyInputUndefined() const
{
	for(uint i = 0; i < inputPins.size(); i++)
	{
		if(inputPins[i]->getValue() == -1) return true;
	}

	return false;
}

void Electronics::setAllOutputsToUndefined()
{
	for(uint i = 0; i < outputPins.size(); i++)
	{
		outputPins[i]->setValue(-1);
	}
}

void Electronics::propagateOutputs()
{
	for(uint i = 0; i < outputPins.size(); i++)
	{
		outputPins[i]->propagate();
	}
}