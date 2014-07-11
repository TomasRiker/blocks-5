#ifndef _ENEMY_H
#define _ENEMY_H

#include "object.h"

/*** Klasse für Feinde ***/

class Enemy : public Object
{
public:
	Enemy(Level& level, const Vec2i& position, int subType, int dir);
	~Enemy();

	void onRender(int layer, const Vec4d& color);
	void onUpdate();
	void onCollect(Player* p_player);
	bool changeInEditor(int mod);
	void saveAttributes(TiXmlElement* p_target);
	void saveExtendedAttributes(TiXmlElement* p_target);
	void loadExtendedAttributes(TiXmlElement* p_element);
	std::string getToolTip() const;
	void setInvisibility(int invisibility);

private:
	bool tryToMove(const Vec2i& dir);
	bool canSee(const Vec2i& what);

	int subType;
	int dir;
	int anim;
	int moveCounter;
	int thinkCounter;
	int soundCounter;
	int eatCounter;
	int burpCounter;
	double shownDir;
	Vec2i targetPosition;
	int interest;
	int contamination;
	double height;
	double vy;
	int invisibility;
};

#endif