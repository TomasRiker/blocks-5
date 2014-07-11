#include "pch.h"
#include "pin.h"
#include "electronics.h"

Pin::Pin(Electronics* p_object,
		 uint pinID,
		 const Vec2i& position,
		 PinType type)
{
	this->p_object = p_object;
	this->pinID = pinID;
	this->position = position;
	this->type = type;
	value = -1;
	oldValue = -1;
}

Pin::~Pin()
{
	// alle Verbindungen lösen
	disconnectAll();
}

const Electronics* Pin::getObject() const
{
	return p_object;
}

uint Pin::getPinID() const
{
	return pinID;
}

const std::set<Pin*>& Pin::getConnectedPins() const
{
	return connectedPins;
}

bool Pin::connect(Pin* p_pin1,
				  Pin* p_pin2)
{
	// Ein Pin kann nicht mit sich selbst verbunden werden.
	if(!p_pin1 || !p_pin2 || p_pin1 == p_pin2) return false;

	// Es können keine Eingänge oder Ausgänge miteinander verbunden werden.
	if(p_pin1->type == p_pin2->type) return false;

	// Ein- und Ausgang bestimmen
	Pin* p_input = p_pin1;
	Pin* p_output = p_pin2;
	if(p_pin2->type == PT_INPUT) p_input = p_pin2;
	if(p_pin1->type == PT_OUTPUT) p_output = p_pin1;

	// Der Eingang darf noch nicht verbunden sein.
	if(p_input->isConnected()) return false;

	// Verbinden!
	p_pin1->connectedPins.insert(p_pin2);
	p_pin2->connectedPins.insert(p_pin1);

	return true;
}

void Pin::disconnect(Pin* p_pin1,
					 Pin* p_pin2)
{
	if(!p_pin1 || !p_pin2 || p_pin1 == p_pin2) return;
	p_pin1->connectedPins.erase(p_pin2);
	p_pin2->connectedPins.erase(p_pin1);

	// Eingang bestimmen
	Pin* p_input = p_pin1;
	if(p_pin2->type == PT_INPUT) p_input = p_pin2;

	// Der Eingang ist jetzt undefiniert.
	p_input->setValue(-1);
}

void Pin::disconnectAll()
{
	while(!connectedPins.empty()) disconnect(this, *connectedPins.begin());
}

bool Pin::isConnected() const
{
	return !connectedPins.empty();
}

std::vector<Vec2f> Pin::getConnectionPath(const Pin* p_pin1,
										  const Pin* p_pin2)
{
	std::vector<Vec2f> path;

	Vec2d x0 = p_pin1->getScreenPosition();
	Vec2d x1 = p_pin2->getScreenPosition();
	const Vec2i& pin1 = p_pin1->getObject()->transformToScreen(p_pin1->getPosition());
	const Vec2i& pin2 = p_pin2->getObject()->transformToScreen(p_pin2->getPosition());
	Vec2d m0, m1;
	if(pin1.x == 0) m0 = Vec2d(-1.0, 0.0);
	else if(pin1.y == 0) m0 = Vec2d(0.0, -1.0);
	else if(pin1.x == 15) m0 = Vec2d(1.0, 0.0);
	else if(pin1.y == 15) m0 = Vec2d(0.0, 1.0);
	if(pin2.x == 0) m1 = Vec2d(-1.0, 0.0);
	else if(pin2.y == 0) m1 = Vec2d(0.0, -1.0);
	else if(pin2.x == 15) m1 = Vec2d(1.0, 0.0);
	else if(pin2.y == 15) m1 = Vec2d(0.0, 1.0);

	m0 *= 60.0;
	m1 *= -60.0;

	// kubisches Spline
	const Vec2d a(m0 + m1 + 2.0 * (x0 - x1));
	const Vec2d b(-2.0 * m0 - m1 - 3.0 * (x0 - x1));
	const Vec2d c(m0);
	const Vec2d d(x0);

	for(int i = 0; i <= 1000; i += 100)
	{
		double t = 0.001 * i;
		const Vec2d p(a * (t * t * t) + b * (t * t) + c * t + d);
		path.push_back(p);
	}

#ifdef EDGY_CONNECTIONS
	Vec2i start = p_pin1->getScreenPosition();
	Vec2i end = p_pin2->getScreenPosition();

	path.push_back(start);

	const Vec2i& pin1 = p_pin1->getObject()->transformToScreen(p_pin1->getPosition());
	const Vec2i& pin2 = p_pin2->getObject()->transformToScreen(p_pin2->getPosition());
	Vec2i dir1, dir2;
	if(pin1.x == 0) dir1 = Vec2i(-1, 0);
	else if(pin1.y == 0) dir1 = Vec2i(0, -1);
	else if(pin1.x == 15) dir1 = Vec2i(1, 0);
	else if(pin1.y == 15) dir1 = Vec2i(0, 1);
	if(pin2.x == 0) dir2 = Vec2i(-1, 0);
	else if(pin2.y == 0) dir2 = Vec2i(0, -1);
	else if(pin2.x == 15) dir2 = Vec2i(1, 0);
	else if(pin2.y == 15) dir2 = Vec2i(0, 1);

	Vec2i cursor = path.front();
	int dist = 4;
	if(p_pin1->getPinID() < 10) dist += p_pin1->getPinID() * 2;
	else if(p_pin1->getPinID() >= 10) dist += (p_pin1->getPinID() - 10) * 2;
	if(p_pin2->getPinID() < 10) dist += p_pin2->getPinID() * 2;
	else if(p_pin2->getPinID() >= 10) dist += (p_pin2->getPinID() - 10) * 2;
	cursor += dir1 * dist;
	path.push_back(cursor);

	Vec2i n = end + dir2 * dist;
	Vec2i d = n - cursor;
	if(abs(d.x) >= abs(d.y))
	{
		cursor.y += d.y;
		path.push_back(cursor);
		cursor.x += d.x;
		path.push_back(cursor);
	}
	else
	{
		cursor.x += d.x;
		path.push_back(cursor);
		cursor.y += d.y;
		path.push_back(cursor);
	}

	path.push_back(end);
#endif

	return path;
}

const Vec2i& Pin::getPosition() const
{
	return position;
}

Vec2i Pin::getScreenPosition() const
{
	return p_object->getShownPositionInPixels() + p_object->transformToScreen(position);
}

PinType Pin::getType() const
{
	return type;
}

int Pin::getValue() const
{
	return value;
}

void Pin::setValue(int value)
{
	// Nur Ausgänge können gesetzt werden.
	if(type != PT_OUTPUT) return;

	this->oldValue = this->value;
	this->value = value;
}

void Pin::propagate()
{
	if(type != PT_OUTPUT) return;

	// den Wert an alle angeschlossenen Pins übertragen
	for(std::set<Pin*>::const_iterator i = connectedPins.begin(); i != connectedPins.end(); ++i)
	{
		(*i)->oldValue = (*i)->value;
		(*i)->value = this->value;
	}
}

int Pin::getOldValue() const
{
	return oldValue;
}

void Pin::writeValue(int value)
{
	this->value = value;
}

void Pin::writeOldValue(int oldValue)
{
	this->oldValue = oldValue;
}