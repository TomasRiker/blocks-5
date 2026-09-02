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
	warpTo(position);
	flags = OF_FIXED | OF_DESTROYABLE | OF_NO_SHADOW;
	this->dir = dir;
	viewDir = Vec2d(0.0, 0.1);
	closed = 0;
}

Eye::~Eye()
{
}

void Eye::updateSprites()
{
	// Auge und, wenn es offen ist, die Pupille darueber. Die Pupille sitzt
	// nicht mittig, sondern schaut dorthin, wo der Spieler steht.
	sprites.add(Vec2i(closed ? 160 : 128, 448)).mirrorX = dir == 1;

	if(!closed)
	{
		sprites.add(Vec2i(192, 448)).offset = Vec2d(0.5, 0.5) + 3.0 * viewDir;
	}
}

void Eye::onRender(int layer,
				   const Vec4d& color)
{
	if(layer == 1) Engine::inst().renderSprites(sprites, color);
}

void Eye::onUpdate()
{
	Player* p_player = level.getActivePlayer();
	if(p_player)
	{
		// Verbindungsvektor zum Spieler suchen
		Vec2d playerPos = static_cast<Vec2d>(p_player->getShownPositionInPixels()) + Vec2d(7.5, 7.5);
		Vec2d myPos = static_cast<Vec2d>(getShownPositionInPixels()) + Vec2d(7.5, 7.5);
		Vec2d targetDir = playerPos - myPos;
		double distSq = targetDir.lengthSq();
		if(distSq > 1.0)
		{
			targetDir.normalize();
			viewDir = 0.9 * viewDir + 0.1 * targetDir;
			viewDir.normalize();
		}

		if(!closed && distSq < 300.0)
		{
			closed = random(100, 105);
			TiXmlElement enemy("");
			enemy.SetAttribute("subType", random(0, 1));
			enemy.SetAttribute("dir", random(0, 3));
			Enemy* p_enemy = static_cast<Enemy*>(level.getPresets()->instancePreset("Enemy", position, &enemy));
			p_enemy->setInvisibility(50);
		}
	}
	else
	{
		viewDir += 0.2 * Vec2d(-viewDir.y, viewDir.x);
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