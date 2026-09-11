#include "pch.h"
#include "teleporter.h"
#include "engine.h"
#include "glstate.h"

Teleporter::Teleporter(Level& level,
					   const Vec2i& position,
					   const Vec2i& targetPosition,
					   int subType) : Object(level, 100)
{
	renderLayers = RL_FLOOR | RL_EDITOR;
	warpTo(position);
	flags = OF_FIXED | OF_NO_SHADOW;
	this->targetPosition = targetPosition;
	this->subType = subType;
	anim = 0;
}

Teleporter::~Teleporter()
{
}

void Teleporter::updateSprites()
{
	// teleporter
	sprites.add(Vec2i((anim / 3 % 8) * 32, 64), subType == 0 ? Vec4d(1.0, 1.0, 1.0, 1.0) : Vec4d(0.0, 1.0, 1.0, 1.0));
}

void Teleporter::onRender(RenderLayer layer,
						  const Vec4d& color)
{
	if(layer == RL_FLOOR) Engine::inst().renderSprites(sprites, color);
	if(layer == RL_EDITOR)
	{
		if(targetPosition != position)
		{
			// mark the target
			GLState::pushEnables();
			GLState::setTexturing(false);
			Vec2i t = (targetPosition - position) * 16 + Vec2i(7, 7);
			glBegin(GL_LINES);
			glColor4d(0.0, 1.0, 0.5, 0.25);
			glVertex2i(7, 7);
			glVertex2i(t.x, t.y);
			Vec2d y(targetPosition - position);
			y.normalize();
			Vec2d x(-y.y, y.x);
			Vec2d p1 = Vec2d(t.x, t.y) - 10.0 * y - 10.0 * x;
			Vec2d p2 = Vec2d(t.x, t.y) - 10.0 * y + 10.0 * x;
			glVertex2d(p1.x, p1.y);
			glVertex2i(t.x, t.y);
			glVertex2i(t.x, t.y);
			glVertex2d(p2.x, p2.y);
			glEnd();
			GLState::popEnables();
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
					Engine::inst().playSound("teleport_failed.ogg", false, 0.0, 100);
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
	switch(subType % 2)
	{
	case 0: return "$TT_TELEPORTER";
	case 1: return "$TT_TELEPORTER_NO_PLAYER";
	}

	return toolTip;
}