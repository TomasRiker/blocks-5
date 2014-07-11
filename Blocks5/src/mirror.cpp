#include "pch.h"
#include "mirror.h"
#include "engine.h"

Mirror::Mirror(Level& level,
			   const Vec2i& position,
			   int subType,
			   int dir) : Object(level, 1)
{
	warpTo(position);
	flags = OF_MASSIVE | OF_DESTROYABLE | OF_TRANSPORTABLE;
	interpolation = 0.3;
	destroyTime = 50;
	if(subType == 0) debrisColor = Vec4d(0.5, 0.5, 1.0, 0.25);
	else debrisColor = Vec4d(0.5, 0.5, 0.6, 0.25);
	this->subType = subType;
	this->dir = dir;
}

Mirror::~Mirror()
{
}

void Mirror::onRender(int layer,
					  const Vec4d& color)
{
	if(layer == 1)
	{
		// Spiegel rendern
		Vec2i positionOnTexture;
		if(subType == 0) positionOnTexture = Vec2i(160, 160);
		else positionOnTexture = Vec2i(160, 352);
		Engine::inst().renderSprite(Vec2i(0, 0), positionOnTexture, Vec2i(16, 16), color, false, 90.0 * dir);
	}
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

bool Mirror::reflectProjectile(Vec2d& velocity)
{
	if(subType != 1) return false;

	double temp;
	const double epsilon = 0.001;
	bool reflected = false;

	switch(this->dir % 4)
	{
	case 0:
		if((velocity.x > epsilon && fabs(velocity.y) < epsilon) ||
		   (fabs(velocity.x) < epsilon && velocity.y > epsilon))
		{
			temp = velocity.x;
			velocity.x = -velocity.y;
			velocity.y = -temp;
			reflected = true;
		}
		break;
	case 1:
		if((velocity.x < -epsilon && fabs(velocity.y) < epsilon) ||
		   (fabs(velocity.x) < epsilon && velocity.y > epsilon))
		{
			temp = velocity.x;
			velocity.x = velocity.y;
			velocity.y = temp;
			reflected = true;
		}
		break;
	case 2:
		if((velocity.x < -epsilon && fabs(velocity.y) < epsilon) ||
		   (fabs(velocity.x) < epsilon && velocity.y < -epsilon))
		{
			temp = velocity.x;
			velocity.x = -velocity.y;
			velocity.y = -temp;
			reflected = true;
		}
		break;
	case 3:
		if((velocity.x > epsilon && fabs(velocity.y) < epsilon) ||
		   (fabs(velocity.x) < epsilon && velocity.y < -epsilon))
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
	switch(subType % 2)
	{
	case 0: return "$TT_MIRROR_LASER";
	case 1: return "$TT_MIRROR_CANNON";
	}

	return toolTip;
}