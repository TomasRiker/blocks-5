#ifndef _E_HEXDIGIT_H
#define _E_HEXDIGIT_H

#include "electronics.h"

/*** Class for a hexadecimal digit display ***/

class E_HexDigit : public Electronics
{
public:
	E_HexDigit(Level& level, const Vec2i& position, int dir);
	~E_HexDigit();

	void onRender(RenderLayer layer, const Vec4d& color);
	void updateSprites();
	bool changeInEditor(int mod);
	void doLogic();

private:
	int value;
};

#endif