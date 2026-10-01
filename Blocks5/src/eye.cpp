#include "pch.h"
#include "eye.h"
#include "engine.h"
#include "player.h"
#include "enemy.h"
#include "presets.h"

Eye::Eye(Level& level,
		 const Vec2i& position,
		 int dir) : Object(level, 2)
{
	renderLayers = RL_MAIN;
	warpTo(position);
	flags = OF_FIXED | OF_DESTROYABLE | OF_NO_SHADOW;
	this->dir = dir > 0 ? 1 : -1;
	viewDir = Vec2f(0.0f, 0.1f);
	closed = 0;
}

Eye::~Eye()
{
}

void Eye::updateSprites()
{
	// The eye and, when it is open, the pupil over it. The pupil does not
	// sit centred; it looks at where the player is standing.
	sprites.add(Vec2i(closed ? 160 : 128, 448)).mirrorX = dir == 1;

	if(!closed)
	{
		// roundToVec2i, not +0.5 and a conversion, which cuts towards zero on
		// the negative side: the pupil would reach three pixels one way and
		// only two the other.
		sprites.add(Vec2i(192, 448)).offset = roundToVec2i(3.0f * viewDir);
	}
}

void Eye::onRender(RenderLayer layer,
				   const Vec4f& color)
{
	if(layer == RL_MAIN) Engine::inst().renderSprites(sprites, color);
}

void Eye::onUpdate()
{
	Player* p_player = level.getActivePlayer();
	if(p_player)
	{
		// find the vector to the player
		Vec2f playerPos = static_cast<Vec2f>(p_player->getShownPositionInPixels()) + Vec2f(7.5f, 7.5f);
		Vec2f myPos = static_cast<Vec2f>(getShownPositionInPixels()) + Vec2f(7.5f, 7.5f);
		Vec2f targetDir = playerPos - myPos;
		float distSq = targetDir.lengthSq();
		if(distSq > 1.0f)
		{
			targetDir.normalize();
			viewDir = 0.9f * viewDir + 0.1f * targetDir;
			viewDir.normalize();
		}

		if(!closed && distSq < 300.0f)
		{
			closed = random(100, 105);
			TiXmlElement enemy("");
			enemy.SetAttribute("subType", random(0, 1));
			enemy.SetAttribute("dir", random(0, 3));
			Enemy* p_enemy = static_cast<Enemy*>(level.getPresets()->instancePreset("Enemy", position, &enemy));
			if(p_enemy) p_enemy->setInvisibility(50);
		}
	}
	else
	{
		viewDir += 0.2f * Vec2f(-viewDir.y, viewDir.x);
		viewDir.normalize();
	}

	if(closed) closed--;
}

bool Eye::changeInEditor(int mod)
{
	dir = -dir;

	return true;
}

void Eye::saveAttributes(TiXmlElement* p_target)
{
	p_target->SetAttribute("dir", dir);
}