#ifndef _E_HEXDIGIT_H
#define _E_HEXDIGIT_H

#include "electronics.h"

/*** Klasse für eine Hexadezimal-Ziffernanzeige ***/

class E_HexDigit : public Electronics
{
public:
	E_HexDigit(Level& level, const Vec2i& position, int dir);
	~E_HexDigit();

	void onRender(int layer, const Vec4d& color);
	bool changeInEditor(int mod);
	void doLogic();

private:
	int value;
};

#endif