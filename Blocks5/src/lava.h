#ifndef _LAVA_H
#define _LAVA_H

#include "object.h"

/*** Klasse für Lava ***/

class Lava : public Object
{
public:
	Lava(Level& level, const Vec2i& position, int dir);
	~Lava();

	void onRender(int layer, const Vec4d& color);
	void onUpdate();
	void frameBegin();
	bool changeInEditor(int mod);
	void saveAttributes(TiXmlElement* p_target);

private:
	void getAlpha1(const Vec2i& where, double* p_out);
	void getAlpha2(const Vec2i& where, double* p_out);

	int anim;
	int dir;
};

#endif