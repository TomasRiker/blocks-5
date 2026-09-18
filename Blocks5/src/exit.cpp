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
	float alpha = 1.0f;
	if(level.getNumDiamondsCollected() >= level.getNumDiamondsNeeded()) alpha = 0.85f + 0.15f * cos(static_cast<float>(level.counter) * 0.4f);
	sprites.add(Vec2i(224, 32), Vec4f(1.0f, 1.0f, 1.0f, alpha));
}

void Exit::onRender(RenderLayer layer,
					const Vec4f& color)
{
	if(ghost) return;
	if(layer == RL_MAIN) Engine::inst().renderSprites(sprites, color);
	else if(layer == RL_LIGHT)
	{
		level.renderShine(0.5f, 0.4f + 0.05f * glowJitter);
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
		Engine::inst().playSound("finished.ogg", false, 0.0f, 100);

		if(level.isInMenu())
		{
			p_obj->disappear(0.25f);
		}
	}
}