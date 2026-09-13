#include "pch.h"
#include "exit.h"
#include "player.h"
#include "engine.h"

Exit::Exit(Level& level,
		   const Vec2i& position) : Object(level, 255)
{
	renderLayers = RL_MAIN | RL_LIGHT;
	warpTo(position);
	flags = OF_FIXED;
	ghost = !level.isInEditor();

	// There can be only one.
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
	// The exit. With enough diamonds it pulses.
	double alpha = 1.0;
	if(level.getNumDiamondsCollected() >= level.getNumDiamondsNeeded()) alpha = 0.85 + 0.15 * cos(static_cast<double>(level.counter) * 0.4);
	sprites.add(Vec2i(224, 32), Vec4d(1.0, 1.0, 1.0, alpha));
}

void Exit::onRender(RenderLayer layer,
					const Vec4d& color)
{
	if(ghost) return;
	if(layer == RL_MAIN) Engine::inst().renderSprites(sprites, color);
	else if(layer == RL_LIGHT)
	{
		level.renderShine(0.5, 0.4 + 0.05 * glowJitter);
	}
}

void Exit::onUpdate()
{
	if(ghost) return;

	// Player here?
	Object* p_obj = level.getFrontObjectAt(position);
	if(p_obj == level.getActivePlayer())
	{
		// Level done.
		level.finished = true;
		Engine::inst().playSound("finished.ogg", false, 0.0, 100);

		if(level.isInMenu())
		{
			p_obj->disappear(0.25);
		}
	}
}