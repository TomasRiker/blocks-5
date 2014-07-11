#ifndef _ELECTRONICS_H
#define _ELECTRONICS_H

#include "object.h"
#include "pin.h"

/*** Klasse für elektronische Bauteile ***/

class Electronics : public Object
{
public:
	Electronics(Level& level, const Vec2i& position, int dir);
	~Electronics();

	static void updateAll(Level& level);

	void onRemove();
	void onRender(int layer, const Vec4d& color);
	void saveAttributes(TiXmlElement* p_target);
	void saveExtendedAttributes(TiXmlElement* p_target);
	void loadExtendedAttributes(TiXmlElement* p_element);
	virtual void doLogic();

	void saveConnections(TiXmlElement* p_target);
	void loadConnections(TiXmlElement* p_source);

	const std::vector<Pin*>& getInputPins() const;
	const std::vector<Pin*>& getOutputPins() const;
	Pin* getPinByID(uint pinID);
	Pin* getPinAt(const Vec2i& where);
	Vec2i transformToScreen(const Vec2i& position) const;
	Vec2i transformFromScreen(const Vec2i& position) const;

protected:
	void createPin(int pinID, const Vec2i& position, PinType pinType);
	int getValue(int pinID);
	int getOldValue(int pinID);
	void setValue(int pinID, int value);
	bool areAllInputsConnected() const;
	bool isAnyInputUndefined() const;
	void setAllOutputsToUndefined();
	void propagateOutputs();

	int dir;
	bool renderBox;
	std::vector<Pin*> inputPins;
	std::vector<Pin*> outputPins;
};

#endif