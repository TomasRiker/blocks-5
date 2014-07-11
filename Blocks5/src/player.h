#ifndef _PLAYER_H
#define _PLAYER_H

#include "object.h"

/*** Klasse für Spieler ***/

class SoundInstance;

class Player : public Object
{
public:
	Player(Level& level, const Vec2i& position, uint character, bool active);
	~Player();

	void onRemove();
	void onRender(int layer, const Vec4d& color);
	void onUpdate();
	bool move(const Vec2i& dir, bool deadlyWeight = false);
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