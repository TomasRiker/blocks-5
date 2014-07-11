#ifndef _STDOBJECT_H
#define _STDOBJECT_H

#include "object.h"

/*** Klasse für Standardobjekte wie Blöcke, Diamanten, Gras ***/

class StdObject : public Object
{
public:
	StdObject(Level& level, const Vec2i& position, uint flags, int depth, const Vec2i& positionOnTexture);
	~StdObject();

	void onRender(int layer, const Vec4d& color);
	void onUpdate();
	void onCollect(Player* p_player);

	void setAnimation(int numFrames, int animSpeed);
	void setCollectData(const std::string& collectSoundFilename, uint inventoryIndex);

	bool getGlow() const;
	void setGlow(bool glow);

private:
	Vec2i positionOnTexture;
	int numFrames;
	int animSpeed;
	int anim;
	std::string collectSoundFilename;
	uint inventoryIndex;
	bool glow;
};

#endif