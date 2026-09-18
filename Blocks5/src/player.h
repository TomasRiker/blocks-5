#ifndef _PLAYER_H
#define _PLAYER_H

#include "object.h"

/*** Class for players ***/

class SoundInstance;

class Player : public Object
{
public:
	Player(Level& level, const Vec2i& position, uint character, bool active);
	~Player();

	void onRemove();
	void onRender(RenderLayer layer, const Vec4f& color);
	void updateSprites();
	void onUpdate();
	bool move(const Vec2i& dir, bool deadlyWeight = false);

	// Whether move() would get anywhere, without doing any of it. It is
	// the question the mouse drag asks before it commands a direction.
	bool canMove(const Vec2i& dir);

	bool changeInEditor(int mod);
	void saveAttributes(TiXmlElement* p_target);
	void saveExtendedAttributes(TiXmlElement* p_target);
	void loadExtendedAttributes(TiXmlElement* p_element);
	std::string getToolTip() const;

	bool isActive() const;
	void activate();
	void deactivate();
	uint getInventory(uint index);
	bool addInventory(uint index, int add);

	int getContamination() const;

	static uint getNumInstances();
	static const std::list<Player*>& getInstances();

	bool censored;

private:
	uint character;
	bool active;
	uint inventory[8];
	Object* p_bomb;
	int walk;
	int touch;
	int push;
	int plantBomb;
	int contamination;

	static uint numInstances;
	static std::list<Player*> instances;
	static SoundInstance* p_toxicSoundInst;
	static SoundInstance* p_maskSoundInst;

	void updateToxicSound();
	void updateMaskSound();
};

#endif