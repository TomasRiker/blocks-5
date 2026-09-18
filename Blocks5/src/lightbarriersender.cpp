#include "pch.h"
#include "lightbarriersender.h"
#include "engine.h"

LightBarrierSender::LightBarrierSender(Level& level,
									   const Vec2i& position,
									   int dir) : Object(level, 1)
{
	renderLayers = RL_MAIN | RL_EFFECT | RL_LIGHT | RL_SPARKLE;
	warpTo(position);
	flags = OF_MASSIVE | OF_FIXED | OF_TRANSPORTABLE;
	this->dir = dir;
	counter = 0;
}

LightBarrierSender::~LightBarrierSender()
{
}

void LightBarrierSender::updateSprites()
{
	sprites.add(Vec2i(64, 608)).rotation = 90.0f * dir;
}

void LightBarrierSender::onRender(RenderLayer layer,
								  const Vec4f& color)
{
	Vec2i sp = getShownPositionInPixels();

	if(layer == RL_MAIN) Engine::inst().renderSprites(sprites, color);
	else if(layer == RL_EFFECT || layer == RL_SPARKLE)
	{
		if(!beam.empty())
		{
			Vec2f oldDir(0.0f);
			Vec2f oldP(0.0f);
			Vec2f dir(0.0f);
			Vec2f p(0.0f);

			beamPoints.clear();

			std::list<Vec2f>::const_iterator last = beam.end();
			last--;
			uint n = 0;
			for(std::list<Vec2f>::const_iterator i = beam.begin(); i != beam.end(); ++i, ++n)
			{
				oldP = p;
				oldDir = dir;
				p = *i;
				dir = p - oldP;

				// The first two points and the last always: the second is
				// where the first real direction exists, and only from the
				// third on does a point that keeps it add nothing.
				if(n < 2 || i == last || dir != oldDir)
				{
					beamPoints.push_back(p);
				}
			}

			// render the inner and the outer beam
			Renderer& renderer = Renderer::inst();
			renderer.push();
			// Onto the cell's centre line, where the lens is - see
			// BEAM_DRAW_OFFSET. The widths below are pixel counts: two for
			// the core, four for the glow.
			renderer.translate(-sp.x + BEAM_DRAW_OFFSET, -sp.y + BEAM_DRAW_OFFSET);

			// The sparkle pass is what the night vision shows of the beam:
			// it is drawn after the quad that darkens everything unlit, so it
			// is the one part not multiplied down by the light field. In
			// daylight this beam is a thin faint thing beside the laser's,
			// deliberately, and scaling that down again the way the laser
			// scales its own leaves nothing to see in the dark. So it is the
			// laser's colours here at half its factor - the same kind of beam
			// a step behind it, rather than the ninth of it that scaling this
			// object's own daylight beam gives - and only its own width is
			// kept.
			float x = static_cast<float>(counter) * 0.8f;
			Vec4f color;
			if(layer == RL_EFFECT) color = Vec4f(1.0f, 0.1f, 0.0f, 0.2f + 0.05f * sin(x));
			else color = Vec4f(0.0f, 0.25f, 0.0f, 0.2f * (0.2f + 0.05f * sin(x)));
			renderer.polyline(beamPoints, 4.0f, color);
			renderer.point(p, 4.0f, color);

			if(layer == RL_EFFECT) color = Vec4f(1.0f, 0.225f + 0.025f * glowJitter, 0.0f, 0.3f + 0.1f * cos(x));
			else color = Vec4f(0.0f, 0.625f + 0.025f * glowJitter, 0.0f, 0.2f * (0.9f + 0.1f * cos(x)));
			renderer.polyline(beamPoints, 2.0f, color);
			renderer.point(p, 2.0f, color);

			renderer.pop();
		}
	}
	else if(layer == RL_LIGHT)
	{
		// A size of 0.3 against the laser's 0.4, which by the relation
		// renderBeamShines describes is three quarters of its light.
		level.renderBeamShines(beam, sp, 0.25f, 0.3f, 0.05f, glowJitter);
	}
}

void LightBarrierSender::onUpdate()
{
	beam.clear();

	// compute the beam
	Vec2i beamDir = numberToDir(dir);
	Vec2f beamPos = Vec2f(7.5f, 7.5f) + getShownPositionInPixels();
	Vec2i beamPosF;
	beam.push_back(beamPos);
	beamPos += beamDir;

	int z = 0;

	while(true)
	{
		beamPosF = beamPos / 16;

		beam.push_back(beamPos);
		if(!level.isValidPosition(beamPosF))
		{
			break;
		}

		bool reflected = false;

		// Is there something at this spot?
		Object* p_obj = 0;
		Vec2i tileHit;
		if(z > 2 && !level.isFreeAt2(beamPos, 0, &p_obj, &tileHit))
		{
			if(p_obj)
			{
				// An object blocks the way.
				if(beamDir.x) beamPos.x = 7.5f + p_obj->getShownPositionInPixels().x;
				else if(beamDir.y) beamPos.y = 7.5f + p_obj->getShownPositionInPixels().y;
				beam.back() = beamPos;

				if(p_obj->reflectLaser(beamDir, true))
				{
					// OK, the object deflected the beam!
					reflected = true;
				}
				else
				{
					beamPos -= 5 * beamDir;
					beam.back() = beamPos;
				}
			}
			else
			{
				// A tile blocks the way.
				if(beamDir.x) beamPos.x = 7.5f + beamPosF.x * 16;
				else if(beamDir.y) beamPos.y = 7.5f + beamPosF.y * 16;
				beamPos -= 5 * beamDir;
				beam.back() = beamPos;
			}

			if(reflected && !beamDir.isZero())
			{
				if(p_obj)
				{
					Object* p_newObjectHit;
					do
					{
						beamPos += beamDir;
						p_newObjectHit = 0;
						level.isFreeAt2(beamPos, 0, &p_newObjectHit, &tileHit);
					} while(p_newObjectHit == p_obj);
				}
			}
			else break;
		}

		beamPos += beamDir * 4;
		z++;
	}

	// Once per tick, as the laser's: the pulse in onRender() runs at one
	// rate whatever the beam's length.
	counter++;
}

bool LightBarrierSender::changeInEditor(int mod)
{
	dir++;
	dir %= 4;

	return true;
}

void LightBarrierSender::saveAttributes(TiXmlElement* p_target)
{
	p_target->SetAttribute("dir", dir);
}