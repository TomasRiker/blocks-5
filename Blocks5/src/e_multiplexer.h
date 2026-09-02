#ifndef _E_MULTIPLEXER_H
#define _E_MULTIPLEXER_H

#include "electronics.h"

/*** Klasse fuer einen Multiplexer ***/

class E_Multiplexer : public Electronics
{
public:
	E_Multiplexer(Level& level, const Vec2i& position, int dir);
	~E_Multiplexer();

	void onRender(int layer, const Vec4d& color);
	void updateSprites();
	void saveExtendedAttributes(TiXmlElement* p_target);
	void loadExtendedAttributes(TiXmlElement* p_element);
	bool changeInEditor(int mod);
	void doLogic();

protected:
	int value;
};

#endif