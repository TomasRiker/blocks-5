#include "pch.h"
#include "crossfade.h"
#include "engine.h"

Crossfade::Crossfade()
{
	Engine& engine = Engine::inst();
	screenSize = engine.getScreenSize();
	screenPow2Size = engine.getScreenPow2Size();
	screenTexelScale = engine.getScreenTexelScale();
}

Crossfade::~Crossfade()
{
}

void Crossfade::render(double t,
					   uint oldImageID,
					   uint newImageID)
{
}
