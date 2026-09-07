#ifndef _HOTEL_H
#define _HOTEL_H

#include "object.h"

/*** Class for a hotel for saving the game ***/

class Font;

class Hotel : public Object
{
public:
	Hotel(Level& level, const Vec2i& position);
	~Hotel();

	void onRender(int layer, const Vec4d& color);
	void updateSprites();
	void onUpdate();
	void onRemove();
	void onSave();

	static Hotel* p_hotelToSave;

private:
	int state;
};

#endif