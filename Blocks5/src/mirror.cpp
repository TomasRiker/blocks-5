#include "pch.h"
#include "mirror.h"
#include "engine.h"

Mirror::Mirror(Level& level,
			   const Vec2i& position,
			   int subType,
			   int dir) : Object(level, 1)
{
	renderLayers = RL_MAIN;
	warpTo(position);
	flags = OF_MASSIVE | OF_DESTROYABLE | OF_TRANSPORTABLE;
	interpolation = 0.3f;
	destroyTime = 50;

	// subType comes out of the level file unchecked (presets.cpp), and only
	// 0 and 1 mean anything: the two reflect methods, the sprite and the
	// tooltip each test for one of the two, so any other value would be a
	// mirror that reflects nothing.
	if(subType < 0 || subType > 1)
	{
		printfLog("+ WARNING: Mirror with subType %d, which does not exist. Treating it as a laser mirror.\n",
				  subType);
		subType = 0;
	}

	this->subType = subType;
	this->dir = dir;
}

Mirror::~Mirror()
{
}

void Mirror::updateSprites()
{
	// mirror
	sprites.add(Vec2i(160, subType == 0 ? 160 : 352)).rotation = 90.0f * dir;
}

void Mirror::onRender(RenderLayer layer,
					  const Vec4f& color)
{
	if(layer == RL_MAIN) Engine::inst().renderSprites(sprites, color);
}

void Mirror::onUpdate()
{
}

bool Mirror::reflectLaser(Vec2i& dir,
						  bool lightBarrier)
{
	if(subType != 0) return false;

	int temp;

	switch(this->dir % 4)
	{
	case 0:
		if((dir.x > 0 && !dir.y) ||
		   (!dir.x && dir.y > 0))
		{
			temp = dir.x;
			dir.x = -dir.y;
			dir.y = -temp;
			return true;
		}
		else return false;
		break;
	case 1:
		if((dir.x < 0 && !dir.y) ||
		   (!dir.x && dir.y > 0))
		{
			temp = dir.x;
			dir.x = dir.y;
			dir.y = temp;
			return true;
		}
		else return false;
		break;
	case 2:
		if((dir.x < 0 && !dir.y) ||
		   (!dir.x && dir.y < 0))
		{
			temp = dir.x;
			dir.x = -dir.y;
			dir.y = -temp;
			return true;
		}
		else return false;
		break;
	case 3:
		if((dir.x > 0 && !dir.y) ||
		   (!dir.x && dir.y < 0))
		{
			temp = dir.x;
			dir.x = dir.y;
			dir.y = temp;
			return true;
		}
		else return false;
		break;
	}

	return false;
}

bool Mirror::reflectProjectile(Vec2f& velocity)
{
	if(subType != 1) return false;

	float temp;
	const float epsilon = 0.001f;
	bool reflected = false;

	switch(this->dir % 4)
	{
	case 0:
		if((velocity.x > epsilon && fabsf(velocity.y) < epsilon) ||
		   (fabsf(velocity.x) < epsilon && velocity.y > epsilon))
		{
			temp = velocity.x;
			velocity.x = -velocity.y;
			velocity.y = -temp;
			reflected = true;
		}
		break;
	case 1:
		if((velocity.x < -epsilon && fabsf(velocity.y) < epsilon) ||
		   (fabsf(velocity.x) < epsilon && velocity.y > epsilon))
		{
			temp = velocity.x;
			velocity.x = velocity.y;
			velocity.y = temp;
			reflected = true;
		}
		break;
	case 2:
		if((velocity.x < -epsilon && fabsf(velocity.y) < epsilon) ||
		   (fabsf(velocity.x) < epsilon && velocity.y < -epsilon))
		{
			temp = velocity.x;
			velocity.x = -velocity.y;
			velocity.y = -temp;
			reflected = true;
		}
		break;
	case 3:
		if((velocity.x > epsilon && fabsf(velocity.y) < epsilon) ||
		   (fabsf(velocity.x) < epsilon && velocity.y < -epsilon))
		{
			temp = velocity.x;
			velocity.x = velocity.y;
			velocity.y = temp;
			reflected = true;
		}
		break;
	}

	return reflected;
}

bool Mirror::changeInEditor(int mod)
{
	if(!mod)
	{
		dir++;
		dir %= 4;
	}
	else
	{
		subType++;
		subType %= 2;
	}

	return true;
}

void Mirror::saveAttributes(TiXmlElement* p_target)
{
	p_target->SetAttribute("subType", subType);
	p_target->SetAttribute("dir", dir);
}

std::string Mirror::getToolTip() const
{
	switch(subType)
	{
	case 0: return "$TT_MIRROR_LASER";
	case 1: return "$TT_MIRROR_CANNON";
	}

	return toolTip;
}