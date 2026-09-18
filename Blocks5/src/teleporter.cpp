#include "pch.h"
#include "teleporter.h"
#include "engine.h"

Teleporter::Teleporter(Level& level,
					   const Vec2i& position,
					   const Vec2i& targetPosition,
					   int subType) : Object(level, 100)
{
	renderLayers = RL_FLOOR | RL_EDITOR;
	warpTo(position);
	flags = OF_FIXED | OF_NO_SHADOW;
	this->targetPosition = targetPosition;

	// subType comes out of the level file unchecked (presets.cpp), and only
	// 0 and 1 mean anything: onUpdate() teleports nobody through any other
	// value, while the sprite and the tooltip would still claim one kind.
	if(subType < 0 || subType > 1)
	{
		printfLog("+ WARNING: Teleporter with subType %d, which does not exist. Treating it as one for everybody.\n",
				  subType);
		subType = 0;
	}

	this->subType = subType;
	anim = 0;
}

Teleporter::~Teleporter()
{
}

void Teleporter::updateSprites()
{
	// teleporter
	sprites.add(Vec2i((anim / 3 % 8) * 32, 64), subType == 0 ? Vec4f(1.0f, 1.0f, 1.0f, 1.0f) : Vec4f(0.0f, 1.0f, 1.0f, 1.0f));
}

void Teleporter::onRender(RenderLayer layer,
						  const Vec4f& color)
{
	if(layer == RL_FLOOR) Engine::inst().renderSprites(sprites, color);
	if(layer == RL_EDITOR)
	{
		if(targetPosition != position)
		{
			// mark the target: a line to it with an arrowhead
			Renderer& renderer = Renderer::inst();
			const Vec4f color(0.0f, 1.0f, 0.5f, 0.25f);
			Vec2i t = (targetPosition - position) * 16 + Vec2i(7, 7);
			const Vec2f tip(static_cast<Vec2f>(t));
			renderer.line(Vec2f(7.0f, 7.0f), tip, 1.0f, color);
			Vec2f y(targetPosition - position);
			y.normalize();
			Vec2f x(-y.y, y.x);
			Vec2f p1 = Vec2f(t.x, t.y) - 10.0f * y - 10.0f * x;
			Vec2f p2 = Vec2f(t.x, t.y) - 10.0f * y + 10.0f * x;
			renderer.line(static_cast<Vec2f>(p1), tip, 1.0f, color);
			renderer.line(tip, static_cast<Vec2f>(p2), 1.0f, color);
		}
	}
}

void Teleporter::onUpdate()
{
	if(level.isElectricityOn())
	{
		// Is there an object on the teleporter that had not been there before?
		// Or is the target position now free for an object whose teleport failed before?
		std::vector<Object*> newObjectsOnMe = level.getObjectsAt(position);
		for(std::vector<Object*>::const_iterator i = newObjectsOnMe.begin(); i != newObjectsOnMe.end(); ++i)
		{
			Object* p_obj = *i;
			if(p_obj == this) continue;

			if(std::find(objectsOnMe.begin(), objectsOnMe.end(), p_obj) == objectsOnMe.end() ||
			   (p_obj->hasTeleportFailed() && level.isFreeAt(targetPosition)))
			{
				if(subType == 0 ||
				   (subType == 1 && p_obj->getType() != "Player" && p_obj->getType() != "Enemy"))
				{
					// teleport the object
					p_obj->teleportTo(targetPosition);
				}
				else
				{
					Engine::inst().playSound("teleport_failed.ogg", false, 0.0f, 100);
				}
			}
		}

		objectsOnMe = newObjectsOnMe;

		anim++;
	}
}

const Vec2i& Teleporter::getTargetPosition() const
{
	return targetPosition;
}

void Teleporter::setTargetPosition(const Vec2i& targetPosition)
{
	this->targetPosition = targetPosition;
}

void Teleporter::saveAttributes(TiXmlElement* p_target)
{
	p_target->SetAttribute("targetX", targetPosition.x);
	p_target->SetAttribute("targetY", targetPosition.y);
	p_target->SetAttribute("subType", subType);
}

std::string Teleporter::getToolTip() const
{
	switch(subType)
	{
	case 0: return "$TT_TELEPORTER";
	case 1: return "$TT_TELEPORTER_NO_PLAYER";
	}

	return toolTip;
}