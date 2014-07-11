#include "pch.h"
#include "lightbarriersender.h"
#include "engine.h"

LightBarrierSender::LightBarrierSender(Level& level,
									   const Vec2i& position,
									   int dir) : Object(level, 1)
{
	warpTo(position);
	flags = OF_MASSIVE | OF_FIXED | OF_TRANSPORTABLE;
	this->dir = dir;
	counter = 0;
	positionOnTexture = Vec2i(64, 608);
}

LightBarrierSender::~LightBarrierSender()
{
}

void LightBarrierSender::onRender(int layer,
								  const Vec4d& color)
{
	Vec2i sp = getShownPositionInPixels();

	if(layer == 1)
	{
		// LightBarrierSender rendern
		Engine::inst().renderSprite(Vec2i(0, 0), positionOnTexture, Vec2i(16, 16), color, false, 90.0 * dir);
	}
	else if(layer == 16 || layer == 17)
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

			// inneren und ‰uﬂeren Strahl rendern
			glPushMatrix();
			glTranslated(-sp.x, -sp.y, 0.0);
			glDisable(GL_TEXTURE_2D);

			double x = static_cast<double>(counter) * 0.8;
			Vec4d color;
			if(layer == 16) color = Vec4d(1.0, 0.1, 0.0, 0.2 + 0.05 * sin(x));
			else color = Vec4d(0.0, 0.1, 0.0, 0.4 * (0.2 + 0.05 * sin(x)));
			line.setWidth(2.5f);
			line.setColor(color);
			line.draw();
			glPointSize(3.0f);
			glBegin(GL_POINTS);
			glColor4dv(color);
			glVertex2dv(p);
			glEnd();

			if(layer == 16) color = Vec4d(1.0, random(0.2, 0.25), 0.0, 0.3 + 0.1 * cos(x));
			else color = Vec4d(0.0, random(0.2, 0.25), 0.0, 0.4 * (0.3 + 0.1 * cos(x)));
			line.setWidth(0.5f);
			line.setColor(color);
			line.draw();
			glPointSize(1.5f);
			glBegin(GL_POINTS);
			glColor4dv(color);
			glVertex2dv(p);
			glEnd();

			glEnable(GL_TEXTURE_2D);
			glPopMatrix();
		}
	}
	else if(layer == 18)
	{
		for(std::list<Vec2d>::const_iterator i = beam.begin(); i != beam.end(); ++i)
		{
			Vec2d p = *i - sp;
			glPushMatrix();
			glTranslated(p.x - 7.5, p.y - 7.5, 0.0);
			level.renderShine(0.25, 0.25 + random(-0.05, 0.05));
			glPopMatrix();
		}
	}
}

void LightBarrierSender::onUpdate()
{
	beam.clear();

	// Strahl berechnen
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

		// Ist an dieser Stelle etwas?
		Object* p_obj = 0;
		Vec2i tileHit;
		if(z > 2 && !level.isFreeAt2(beamPos, 0, &p_obj, &tileHit))
		{
			if(p_obj)
			{
				// Ein Objekt versperrt den Weg.
				if(beamDir.x) beamPos.x = 7.5 + p_obj->getShownPositionInPixels().x;
				else if(beamDir.y) beamPos.y = 7.5 + p_obj->getShownPositionInPixels().y;
				beam.back() = beamPos;

				if(p_obj->reflectLaser(beamDir, true))
				{
					// OK, das Objekt hat den Strahl umgelenkt!
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
				// Ein Tile versperrt den Weg.
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