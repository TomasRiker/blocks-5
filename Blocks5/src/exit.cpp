#include "pch.h"
#include "exit.h"
#include "player.h"
#include "engine.h"

Exit::Exit(Level& level,
		   const Vec2i& position) : Object(level, 255)
{
	warpTo(position);
	flags = OF_FIXED;
	ghost = !level.isInEditor();

	// Es kann nur einen geben!
	if(level.p_exit) level.removeObject(level.p_exit);
	level.p_exit = this;
}

Exit::~Exit()
{
}

void Exit::onRemove()
{
	level.p_exit = 0;
}

void Exit::updateSprites()
{
	// Ausgang. Bei genug Diamanten pulsiert er.
	double alpha = 1.0;
	if(level.getNumDiamondsCollected() >= level.getNumDiamondsNeeded()) alpha = 0.85 + 0.15 * cos(static_cast<double>(level.counter) * 0.4);
	sprites.add(Vec2i(224, 32), Vec4d(1.0, 1.0, 1.0, alpha));
}

void Exit::onRender(int layer,
					const Vec4d& color)
{
	if(ghost) return;
	if(layer == 1) Engine::inst().renderSprites(sprites, color);
	else if(layer == 18)
	{
		level.renderShine(0.5, 0.4 + random(-0.05, 0.05));
	}
}

void Exit::onUpdate()
{
	if(ghost) return;

	// Spieler da?
	Object* p_obj = level.getFrontObjectAt(position);
	if(p_obj == level.getActivePlayer())
	{
		// Level geschafft!
		level.finished = true;
		Engine::inst().playSound("finished.ogg", false, 0.0, 100);

		if(level.isInMenu())
		{
			p_obj->disappear(0.25);
		}
	}
}