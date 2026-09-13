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
	sprites.add(Vec2i(64, 608)).rotation = 90.0 * dir;
}

void LightBarrierSender::onRender(RenderLayer layer,
								  const Vec4d& color)
{
	Vec2i sp = getShownPositionInPixels();

	if(layer == RL_MAIN) Engine::inst().renderSprites(sprites, color);
	else if(layer == RL_EFFECT || layer == RL_SPARKLE)
	{
		if(!beam.empty())
		{
			Vec2d oldDir(0.0);
			Vec2d oldP(0.0);
			Vec2d dir;
			Vec2d p;

			line.clear();

			std::list<Vec2d>::const_iterator last = beam.end();
			last--;
			for(std::list<Vec2d>::const_iterator i = beam.begin(); i != beam.end(); ++i)
			{
				oldP = p;
				oldDir = dir;
				p = *i;
				dir = p - oldP;

				if(i == beam.begin() || i == last || dir != oldDir)
				{
					line.addPoint(p);
				}
			}

			// render the inner and the outer beam
			glPushMatrix();
			glTranslated(-sp.x, -sp.y, 0.0);
			// Raw geometry, so the queued sprites have to go up first: they
			// belong underneath it.
			Engine::inst().flushSprites();
			GL::setTexturing(false);

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
			double x = static_cast<double>(counter) * 0.8;
			Vec4d color;
			if(layer == RL_EFFECT) color = Vec4d(1.0, 0.1, 0.0, 0.2 + 0.05 * sin(x));
			else color = Vec4d(0.0, 0.25, 0.0, 0.2 * (0.2 + 0.05 * sin(x)));
			line.setWidth(2.5f);
			line.setColor(color);
			line.draw();
			glPointSize(3.0f);
			glBegin(GL_POINTS);
			glColor4dv(color);
			glVertex2dv(p);
			glEnd();

			if(layer == RL_EFFECT) color = Vec4d(1.0, 0.225 + 0.025 * glowJitter, 0.0, 0.3 + 0.1 * cos(x));
			else color = Vec4d(0.0, 0.625 + 0.025 * glowJitter, 0.0, 0.2 * (0.9 + 0.1 * cos(x)));
			line.setWidth(0.5f);
			line.setColor(color);
			line.draw();
			glPointSize(1.5f);
			glBegin(GL_POINTS);
			glColor4dv(color);
			glVertex2dv(p);
			glEnd();

			GL::setTexturing(true);
			glPopMatrix();
		}
	}
	else if(layer == RL_LIGHT)
	{
		// A size of 0.3 against the laser's 0.4, which by the relation
		// renderBeamShines describes is three quarters of its light.
		level.renderBeamShines(beam, sp, 0.25, 0.3, 0.05);
	}
}

void LightBarrierSender::onUpdate()
{
	beam.clear();

	// compute the beam
	Vec2i beamDir = numberToDir(dir);
	Vec2d beamPos = Vec2d(7.5, 7.5) + getShownPositionInPixels();
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
				if(beamDir.x) beamPos.x = 7.5 + p_obj->getShownPositionInPixels().x;
				else if(beamDir.y) beamPos.y = 7.5 + p_obj->getShownPositionInPixels().y;
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
				if(beamDir.x) beamPos.x = 7.5 + beamPosF.x * 16;
				else if(beamDir.y) beamPos.y = 7.5 + beamPosF.y * 16;
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

		counter++;
	}
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