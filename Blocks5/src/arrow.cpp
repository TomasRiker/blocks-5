#include "pch.h"
#include "arrow.h"
#include "player.h"
#include "engine.h"

Arrow::Arrow(Level& level,
			 const Vec2i& position,
			 int dir) : Object(level, 255)
{
	warpTo(position);
	flags = OF_MASSIVE | OF_FIXED | OF_ARROWTYPE;
	this->dir = dir;
	shownDir = dir;
	dirVel = 0.0;
	shownAlpha = 0.6;
	counter = 0;
}

Arrow::~Arrow()
{
}

void Arrow::onRender(int layer,
					 const Vec4d& color)
{
	if(layer == 1)
	{
		// Pfeil rendern
		Vec2i positionOnTexture(96, 0);
		Engine::inst().renderSprite(Vec2i(0, 0), positionOnTexture, Vec2i(16, 16), color * Vec4d(1.0, 1.0, 1.0, shownAlpha), false, 90.0 * shownDir);
	}
}

void Arrow::onUpdate()
{
	shownDir += dirVel * 0.02;
	double d = static_cast<double>(dir) - shownDir;
	dirVel += d * 2.0;
	dirVel *= 0.9;

	bool active = false;
	Player* p_player = level.getActivePlayer();
	if(p_player)
	{
		const Vec2i& pp = p_player->getPosition();
		switch(dir % 4)
		{
		case 0:
			if(pp == position + Vec2i(0, 1)) active = true;
			break;
		case 1:
			if(pp == position + Vec2i(-1, 0)) active = true;
			break;
		case 2:
			if(pp == position + Vec2i(0, -1)) active = true;
			break;
		case 3:
			if(pp == position + Vec2i(1, 0)) active = true;
			break;
		}
	}

	double alpha = 0.6;
	if(active)
	{
		if((counter / 20) % 2) alpha = 0.8;
		else alpha = 1.0;
		counter++;
	}
	else counter = 0;

	const double interpolation = 0.3;
	shownAlpha = shownAlpha * (1.0 - interpolation) + alpha * interpolation;
}

bool Arrow::allowMovement(const Vec2i& dir)
{
	switch(this->dir % 4)
	{
	case 0:
		return !dir.x && dir.y < 0;
	case 1:
		return dir.x > 0 && !dir.y;
	case 2:
		return !dir.x && dir.y > 0;
	case 3:
		return dir.x < 0 && !dir.y;
	}

	return false;
}

bool Arrow::changeInEditor(int mod)
{
	dir++;
	dir %= 4;
	shownDir = dir;

	return true;
}

void Arrow::saveAttributes(TiXmlElement* p_target)
{
	p_target->SetAttribute("dir", dir);
}

void Arrow::turn()
{
	dir++;
}