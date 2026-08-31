#ifndef _ARROW_H
#define _ARROW_H

#include "object.h"

/*** Klasse fuer einen Durchgangspfeil ***/

class Arrow : public Object
{
public:
	// shownDir dreht sich weich; fuer die Truemmer reicht die naechste Vierteldrehung.
	virtual int getSpriteQuarterTurns() const { return static_cast<int>(shownDir + 0.5); }

	Arrow(Level& level, const Vec2i& position, int dir);
	~Arrow();

	void onRender(int layer, const Vec4d& color);
	void onUpdate();
	bool allowMovement(const Vec2i& dir);
	bool changeInEditor(int mod);
	void saveAttributes(TiXmlElement* p_target);

	void turn();

private:
	int dir;
	double shownDir;
	double dirVel;
	double shownAlpha;
	int counter;
};

#endif