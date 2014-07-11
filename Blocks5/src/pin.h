#ifndef _PIN_H
#define _PIN_H

/*** Klasse für Anschlüsse ***/

enum PinType
{
	PT_INPUT = 0,
	PT_OUTPUT = 1
};

class Electronics;

class Pin
{
public:
	Pin(Electronics* p_object, uint pinID, const Vec2i& position, PinType type);
	~Pin();

	const Electronics* getObject() const;
	uint getPinID() const;
	const std::set<Pin*>& getConnectedPins() const;
	static bool connect(Pin* p_pin1, Pin* p_pin2);
	static void disconnect(Pin* p_pin1, Pin* p_pin2);
	void disconnectAll();
	bool isConnected() const;
	static std::vector<Vec2f> getConnectionPath(const Pin* p_pin1, const Pin* p_pin2);
	const Vec2i& getPosition() const;
	Vec2i getScreenPosition() const;
	PinType getType() const;
	int getValue() const;
	void setValue(int value);
	void propagate();
	int getOldValue() const;
	void writeValue(int value);
	void writeOldValue(int oldValue);

private:
	Electronics* p_object;
	uint pinID;
	std::set<Pin*> connectedPins;
	Vec2i position;
	PinType type;
	int oldValue;
	int value;
};

#endif