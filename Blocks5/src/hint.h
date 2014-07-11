#ifndef _HINT_H
#define _HINT_H

#include "object.h"

/*** Klasse für einen Hinweiszettel ***/

class Font;
class Texture;
class Player;

class Hint : public Object
{
public:
	Hint(Level& level, const Vec2i& position, const std::string& text);
	~Hint();

	void onRender(int layer, const Vec4d& color);
	void onUpdate();
	void onCollect(Player* p_player);
	void saveAttributes(TiXmlElement* p_target);

	const std::string& getText() const;
	void setText(const std::string& text);

private:
	std::string text;
	double alpha;
	double shownAlpha;
	Font* p_font;
	Texture* p_sprite;
	Vec2i targetPosition;
};

#endif