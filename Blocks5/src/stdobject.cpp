#include "pch.h"
#include "stdobject.h"
#include "player.h"
#include "engine.h"

StdObject::StdObject(Level& level,
					 const Vec2i& position,
					 uint flags,
					 int depth,
					 const Vec2i& positionOnTexture) : Object(level, depth)
{
	warpTo(position);
	this->flags = flags;
	this->positionOnTexture = positionOnTexture;
	interpolation = 0.3;
	numFrames = 1;
	animSpeed = 1;
	anim = random(0, 100000);
	inventoryIndex = ~0;
	glow = false;
}

StdObject::~StdObject()
{
}

void StdObject::onRender(int layer,
						 const Vec4d& color)
{
	if(layer == 1)
	{
		// Objekt rendern
		int frame = (anim / animSpeed) % numFrames;
		if(level.isInEditor()) frame = 0;
		Engine::inst().renderSprite(Vec2i(0, 0), positionOnTexture + Vec2i(frame * 32, 0), Vec2i(16, 16), color);
	}
	else if(layer == 18 && glow)
	{
		level.renderShine(0.35, 0.35 + random(-0.05, 0.05));
	}
}

void StdObject::onUpdate()
{
	anim++;
}

void StdObject::onCollect(Player* p_player)
{
	if(inventoryIndex != ~0)
	{
		// dem Spieler den Gegenstand geben
		if(!p_player->addInventory(inventoryIndex, 1)) return;
	}

	if(collectSoundFilename.length())
	{
		// Sound abspielen
		Engine::inst().playSound(collectSoundFilename, false, 0.15, 100);
	}

	disappear(0.2);
}

void StdObject::setAnimation(int numFrames,
							 int animSpeed)
{
	this->numFrames = numFrames;
	this->animSpeed = animSpeed;
}

void StdObject::setCollectData(const std::string& collectSoundFilename,
							   uint inventoryIndex)
{
	this->collectSoundFilename = collectSoundFilename;
	this->inventoryIndex = inventoryIndex;
}

bool StdObject::getGlow() const
{
	return glow;
}

void StdObject::setGlow(bool glow)
{
	this->glow = glow;
}