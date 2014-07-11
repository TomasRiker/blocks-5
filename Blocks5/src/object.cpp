#include "pch.h"
#include "object.h"
#include "conveyorbelt.h"
#include "elevator.h"
#include "player.h"
#include "tileset.h"
#include "particlesystem.h"
#include "engine.h"
#include "gui.h"
#include "font.h"

int Object::nextFallingDepth = 1000000;

Object::Object(Level& level,
			   int depth) : level(level), depth(depth)
{
	level.addObject(this);
	position = shownPosition = Vec2i(0, 0);
	positionOnTexture = Vec2i(-1, -1);
	noCollect = 0.0;
	flags = 0;
	ghost = false;
	destroyTime = 0;
	deathCountDown = 1.0;
	deathSpeed = 0.0;
	newDeathCountDown = 1.0;
	newDeathSpeed = 0.0;
	newDeathTime = -1;
	interpolation = 0.3;
	collisionSound = "";
	moved = false;
	lastMoveDir = Vec2i(0, 0);
	teleporting = 0.0;
	teleportFailed = false;
	oldDepth = depth;
	falling = 0.0;
	mass = 0;
	uid = 0;
	fall = 0;
	lastHashedAt = -1;
	sayText = "";
	sayTime = 0.0;
	sayAlpha = 0.0;
	shadowPass = false;
	slideDir = -1;
	slideMove = false;
	onConveyorBelt = 0;
}

Object::~Object()
{
}

void Object::render(int layer,
					const Vec2i& offset,
					const Vec4d& color)
{
	Vec4d realColor(color.r, color.g, color.b, color.a * deathCountDown);

	glPushMatrix();

	if(!(flags & OF_PROXY))
	{
		if(layer == 939) glTranslated(offset.x, offset.y, 0.0);
		else
		{
			Vec2i sp = getShownPositionInPixels();
			glTranslated(sp.x + offset.x, sp.y + offset.y, 0.0);
		}

		if(layer != 18)
		{
			double o = -16.0;
			if(getType() == "Enemy") o = -17.0;

			if(teleporting > 0.0)
			{
				double y = 1.0 + teleporting * teleporting * 500.0;
				glTranslated(0.0, y * o + 16.0, 0.0);
				glScaled(1.0, y, 1.0);
				realColor *= 1.0 - teleporting;
			}
			else if(teleporting < 0.0)
			{
				double y = 1.0 + teleporting * teleporting * 500.0;
				glTranslated(0.0, y * o + 16.0, 0.0);
				glScaled(1.0, y, 1.0);
				realColor *= 1.0 + teleporting;
			}
			else if(falling > 0.0)
			{
				double f = 1.0 / (1.0 + 4.0 * falling);
				double size = 16.0 * f;
				double add = (16.0 - size) * 0.5;
				glTranslated(add, add, 0.0);
				glScaled(f, f, 1.0);
				glRotated(falling * 120.0, 0.0, 0.0, 1.0);
				realColor.r *= f;
				realColor.g *= f;
				realColor.b *= f;
			}
		}
	}

	onRender(layer, realColor);

	if(layer == 42 &&
	   sayTime > 0.0 &&
	   !sayText.empty())
	{
		Font* p_font = GUI::inst().getFont();

		Font::Options options = p_font->getOptions();
		options.italic = 4;
		p_font->pushOptions();
		p_font->setOptions(options);

		Vec2i dim;
		std::string str = localizeString(sayText);
		p_font->measureText(str, &dim, 0);
		dim += Vec2i(10, 10);

		Vec2i p(getShownPositionInPixels() + offset);
		int mirrorX = p.x + 20 + 15 + dim.x - 25 >= 640 ? -1 : 1;
		int mirrorY = p.y + 20 + 15 + dim.y >= 400 ? -1 : 1;

		glPushMatrix();

		glTranslated(8.0, 8.0, 0.0);
		if(mirrorX == -1) glScaled(-1.0, 1.0, 1.0);
		if(mirrorY == -1) glScaled(1.0, -1.0, 1.0);
		glTranslated(8.0, 8.0, 0.0);

		glDisable(GL_LINE_SMOOTH);
		glDisable(GL_TEXTURE_2D);
		glLineWidth(1.0f);

		glBegin(GL_TRIANGLES);
		glColor4d(0.75, 0.75, 1.0, 0.85 * sayAlpha);
		glVertex2i(2, 2);
		glVertex2i(15, 15);
		glVertex2i(5, 15);
		glEnd();
		glBegin(GL_QUADS);
		glVertex2i(-10, 15);
		glVertex2i(15 + dim.x - 25, 15);
		glColor4d(0.75, 0.75, 1.0, 0.6 * sayAlpha);
		glVertex2i(15 + dim.x - 25, 15 + dim.y);
		glVertex2i(-10, 15 + dim.y);
		glEnd();

		glBegin(GL_LINE_LOOP);
		glColor4d(0.0, 0.0, 0.0, 0.9 * sayAlpha);
		glVertex2i(2, 2);
		glVertex2i(15, 15);
		glVertex2i(15 + dim.x - 25, 15);
		glVertex2i(15 + dim.x - 25, 15 + dim.y);
		glVertex2i(-10, 15 + dim.y);
		glVertex2i(-10, 15);
		glVertex2i(5, 15);
		glEnd();

		glPopMatrix();

		Vec2i textPosition(3 * mirrorX, 28 * mirrorY);
		if(mirrorX == -1) textPosition.x -= dim.x - 10;
		if(mirrorY == -1) textPosition.y -= dim.y - 10;
		p_font->renderText(str, Vec2i(8, 7) + textPosition, Vec4d(1.0, 1.0, 1.0, sayAlpha));

		p_font->popOptions();

		glEnable(GL_LINE_SMOOTH);
	}

	glPopMatrix();
}

void Object::update()
{
	if(noCollect > 0.0)
	{
		noCollect -= 0.02;
		if(noCollect < 0.0) noCollect = 0.0;
	}

	if(sayTime > 0.0)
	{
		if(sayTime < 0.25) sayAlpha = sayTime * 4.0;
		else
		{
			sayAlpha += 4.0 * 0.02;
			sayAlpha = min(sayAlpha, 1.0);
		}

		sayTime -= 0.02;
		if(sayTime < 0.0)
		{
			sayText = "";
			sayTime = 0.0;
			sayAlpha = 0.0;
		}
	}

	if((flags & OF_PROXY))
	{
		// Objekt aktualisieren
		onUpdate();

		// Todes-Countdown runterzählen
		deathCountDown -= deathSpeed * 0.02;
		return;
	}

	if(isAlive() &&
	   teleporting == 0.0 &&
	   falling == 0.0)
	{
		if(fall == -1)
		{
			fall = 0;

			// Kollisions-Sound spielen
			Engine::inst().playSound(collisionSound, false, 0.15, -100);

			// Das Objekt ist mit einem anderen Objekt kollidiert. Welches ist es?
			Object* p_obj = level.getFrontObjectAt(position + Vec2i(0, 1));
			if(p_obj)
			{
				if(flags & OF_DEADLY)
				{
					// Kann das Objekt platzen?
					if(p_obj->flags & OF_BURSTABLE)
					{
						// Ja, dann soll es das jetzt auch tun.
						p_obj->burst();
					}
				}

				this->onCollision(p_obj);
				p_obj->onCollision(this);
			}
		}

		if(!(level.counter % 4))
		{
			if(flags & OF_GRAVITY)
			{
				// Gravitation
				move(Vec2i(0, 1));
			}

			if(slideDir != -1)
			{
				// Rutschen
				slideMove = true;
				if(type == "Player") static_cast<Player*>(this)->move(intToDir(slideDir));
				else move(intToDir(slideDir), mass);
				slideMove = false;
			}
		}

		// Ist das Objekt einsammelbar, und ist ein Spieler hier?
		if(flags & OF_COLLECTABLE && noCollect == 0.0)
		{
			std::vector<Object*> objects = level.getObjectsAt2(position);
			for(std::vector<Object*>::const_iterator it = objects.begin();
				it != objects.end();
				++it)
			{
				if((*it)->getType() == "Player")
				{
					Player* p_player = static_cast<Player*>(*it);
					Vec2i d = p_player->getShownPositionInPixels() - getShownPositionInPixels();
					if(d.lengthSq() <= 36)
					{
						onCollect(p_player);
						break;
					}
				}
			}
		}

		// Objekt aktualisieren
		onUpdate();
	}

	// gezeigte Position aktualisieren
	double i;
	if(level.isElectricityOn() && level.getElevatorAt(position)) i = 0.12;
	else i = interpolation;

	if(onConveyorBelt)
	{
		i = 0.12;
		onConveyorBelt--;
	}

	if(i == -1.0)
	{
		Vec2d dir = Vec2d(position) - shownPosition;
		double l = dir.length();
		double stepSize = 0.02 * (50.0 / 15.0);

		if(l >= stepSize)
		{
			dir /= l;
			shownPosition += stepSize * dir;
		}
	}
	else
	{
		shownPosition = shownPosition * (1.0 - i) + Vec2d(position) * i;
	}

	// Todes-Countdown runterzählen
	if(newDeathTime != -1 && level.time >= newDeathTime)
	{
		deathCountDown = newDeathCountDown;
		deathSpeed = newDeathSpeed;
		newDeathTime = -1;
	}

	deathCountDown -= deathSpeed * 0.02;

	if(teleporting != 0.0)
	{
		ParticleSystem* p_particleSystem = level.getParticleSystem();
		ParticleSystem::Particle p;

		double x;
		if(teleporting > 0.0) x = teleporting;
		else x = teleporting + 1.0;

		if(x <= 0.5)
		{
			double r = 7.0 + 15.0 * x;
			Vec2d o(r * sin(x * 18.0), -r * cos(x * 18.0));

			p.lifetime = 60;
			p.damping = 0.9f;
			p.gravity = 0.0f;
			p.positionOnTexture = Vec2b(0, 0);
			p.sizeOnTexture = Vec2b(16, 16);
			p.position = position * 16 + Vec2i(8, 8) + o;
			p.velocity = o * 0.01f;
			p.color = Vec4d(0.1, 0.1, 1.0, 0.6);
			p.deltaColor = Vec4d(2.0, 1.0, -2.0, 0.4) / p.lifetime;
			p.rotation = random(0.0f, 10.0f);
			p.deltaRotation = random(-0.1f, 0.1f);
			p.size = random(0.5f, 0.75f);
			p.deltaSize = -p.size / p.lifetime;
			p_particleSystem->addParticle(p);
		}

		// teleportieren
		if(teleporting > 0.0)
		{
			teleporting += 0.02;
			if(teleporting > 1.0)
			{
				// Position verändern
				if(level.isFreeAt(teleportingTo))
				{
					warpTo(teleportingTo);
					teleportFailed = false;
					fall = 0;
					slideDir = -1;
				}
				else
				{
					teleportFailed = true;
					Engine::inst().playSound("teleport_failed.ogg", false, 0.0, 100);

					// glühende Partikel
					for(int i = 0; i < 100; i++)
					{
						p.lifetime = 100;
						p.damping = 0.9f;
						p.gravity = 0.05f;
						p.positionOnTexture = Vec2b(32, 32);
						p.sizeOnTexture = Vec2b(16, 16);
						p.position = teleportingTo * 16 + Vec2i(random(6, 10), random(6, 10));
						const double r = random(0.0, 6.283);
						p.velocity = random(3.0, 6.0) * Vec2d(sin(r), cos(r));
						p.color = Vec4d(random(0.5, 1.0), random(0.5, 1.0), 0.0, 0.9);
						p.deltaColor = Vec4d(0.5, 0.0, 0.0, -p.color.a / p.lifetime);
						p.rotation = random(0.0f, 10.0f);
						p.deltaRotation = random(-0.1f, 0.1f);
						p.size = random(0.15f, 0.25f);
						p.deltaSize = random(-0.01f, -0.005f);
						p_particleSystem->addParticle(p);
					}
				}

				teleporting = -1.0;
				Engine::inst().playSound("teleport_end.ogg", false, 0.0, 100);
			}
		}
		else if(teleporting < 0.0)
		{
			teleporting += 0.02;
			if(teleporting > 0.0)
			{
				teleporting = 0.0;
				setDepth(oldDepth);
			}
		}
	}

	if(teleporting == 0.0 && falling == 0.0 && isAlive() && !ghost)
	{
		// Ist unter uns ein Loch?
		uint tileID = level.getTileAt(0, position);
		const TileSet::TileInfo& tileInfo = level.getTileSet()->getTileInfo(tileID);
		if(tileInfo.type == 3)
		{
			bool dontFall = false;

			if(flags & OF_DONT_FALL) dontFall = true;

			// Schienen fallen nicht.
			else if(flags & OF_RAIL) dontFall = true;

			// Objekte auf Aufzügen fallen nicht.
			else if(!(flags & OF_ELEVATOR) && (flags & OF_TRANSPORTABLE) && level.getElevatorAt(position)) dontFall = true;

			// Aufzüge auf Schienen fallen nicht.
			else if(flags & OF_ELEVATOR)
			{
				Object* p_back = level.getBackObjectAt(position);
				if(p_back)
				{
					if(p_back->flags & OF_RAIL) dontFall = true;
				}
			}

			// Objekte, die einen Aufzug knapp verpasst haben, kriegen nochmal eine Chance.
			else if(flags & OF_TRANSPORTABLE)
			{
				// die umgebenden 4 Felder nach Aufzügen absuchen
				std::list<Elevator*> elevators;
				Elevator* p_elevator = level.getElevatorAt(position + Vec2i(-1, 0)); if(p_elevator) elevators.push_back(p_elevator);
				p_elevator = level.getElevatorAt(position + Vec2i(1, 0)); if(p_elevator) elevators.push_back(p_elevator);
				p_elevator = level.getElevatorAt(position + Vec2i(0, -1)); if(p_elevator) elevators.push_back(p_elevator);
				p_elevator = level.getElevatorAt(position + Vec2i(0, 1)); if(p_elevator) elevators.push_back(p_elevator);

				// Entfernungen bestimmen
				Elevator* p_closestElevator = 0;
				double closestDist = 0.0;
				for(std::list<Elevator*>::const_iterator i = elevators.begin(); i != elevators.end(); ++i)
				{
					if((*i)->getPosition() != position - lastMoveDir)
					{
						double dist = ((*i)->shownPosition - position).lengthSq();
						if(!p_closestElevator || dist < closestDist) p_closestElevator = *i, closestDist = dist;
					}
				}

				if(p_closestElevator)
				{
					// Ist der nächste Aufzug nah genug?
					if(closestDist <= 0.5)
					{
						Vec2i d = p_closestElevator->getPosition() - position;
						Vec2i d1, d2;
						if(lastMoveDir.x) d1 = Vec2i(0, d.y), d2 = Vec2i(d.x, 0);
						else d1 = Vec2i(d.x, 0), d2 = Vec2i(0, d.y);

						// das Objekt auf den Aufzug schieben
						moved = false;
						if(getType() == "Player")
						{
							Player* p_this = static_cast<Player*>(this);
							if(p_this->move(d1))
							{
								moved = false;
								if(p_this->move(d2)) dontFall = true;
							}
						}
						else
						{
							if(move(d1))
							{
								moved = false;
								if(move(d2)) dontFall = true;
							}
						}

						position = p_closestElevator->getPosition();
						level.hashObject(this);
					}
				}
			}

			if(!dontFall)
			{
				// Ja, fallen!
				Engine::inst().playSound("falling.ogg", false, 0.0, 100);
				falling = 0.0001;
				ghost = true;
				setDepth(nextFallingDepth--);
			}
		}
	}
	else if(falling > 0.0)
	{
		falling += 0.02;
		if(falling > 1.0 && isAlive()) disappear(0.2);
	}
}

void Object::onRemove()
{
}

void Object::onRender(int layer,
					  const Vec4d& color)
{
}

void Object::onUpdate()
{
}

void Object::onElectricitySwitch(bool on)
{
}

void Object::onCollect(Player* p_player)
{
	disappear(0.2);
}

void Object::onTouchedByPlayer(Player* p_player)
{
}

void Object::onCollision(Object* p_obj)
{
}

void Object::onExplosion()
{
	if(flags & OF_DESTROYABLE)
	{
		// Objekt zerstören
		disappear(0.2);
	}
}

bool Object::move(const Vec2i& dir,
				  uint force)
{
	if(slideDir != -1 && !slideMove) return false;

	if(dir.isZero()) return true;
	if(force < mass || moved || teleporting != 0.0) return false;
	if(!level.isValidPosition(position + dir)) return false;

	if(dir.x && !dir.y && isPushedFromAbove() && level.isElectricityOn())
	{
		// Liegt das Objekt auf einem Fließband?
		Object* p_obj = level.getFrontObjectAt(position + Vec2i(0, 1));
		if(p_obj)
		{
			if(p_obj->getType() == "ConveyorBelt")
			{
				ConveyorBelt* p_cb = reinterpret_cast<ConveyorBelt*>(p_obj);
				if(p_cb->getDir() != dir.x)
				{
					// Schieben gegen Fließbandrichtung nicht erlaubt!
					return false;
				}
			}
		}
	}

	Vec2i oldPosition = position;

	Vec2i np = position + dir;
	int tileType = 0;
	if(level.isFreeAt(np, &tileType))
	{
		position = np;
		moved = true;
	}
	else if(tileType != 1 && tileType != 2)
	{
		Object* p_obj = level.getFrontObjectAt(position + dir);
		if(p_obj)
		{
			// Sind wir ein einsammelbares Objekt und das andere Objekt der Spieler?
			if((flags & OF_COLLECTABLE) && p_obj->getType() == "Player")
			{
				// Dann ist es OK.
				position += dir;
				moved = true;
			}
			// Sind wir ein Aufzug und das andere Objekt transportierbar?
			else if((flags & OF_ELEVATOR) && (p_obj->getFlags() & OF_TRANSPORTABLE))
			{
				// Dann ist es OK.
				position += dir;
				moved = true;
			}
			// Kann sich dieses Objekt bewegen?
			else if(!(p_obj->getFlags() & OF_FIXED) && !p_obj->isPushedWithDeadlyWeight())
			{
				if(p_obj->isPushedFromAbove() && dir.y < 0)
				{
					// Nach oben schieben geht nicht, wenn das Objekt nach unten gedrückt wird!
				}
				else
				{
					p_obj->onConveyorBelt = max(onConveyorBelt, p_obj->onConveyorBelt);

					if(p_obj->move(dir, force - mass))
					{
						lastMoveDir = dir;
						handleSliding();

						if(slideDir < 0)
						{
							position += dir;
							moved = true;
						}
						else
						{
							slideDir = -2;
						}
					}
				}
			}

			if(!moved && !dir.x && dir.y == 1)
			{
				// Wird das Objekt mit extremer Kraft gedrückt?
				if(isPushedWithDeadlyWeight())
				{
					// Ist unter dem "tödlichen" Objekt ein Objekt?
					Object* p_obj = level.getFrontObjectAt(position + Vec2i(0, 1));
					if(p_obj)
					{
						if(p_obj->getType() == "Player")
						{
							Player* p = static_cast<Player*>(p_obj);
							if(!p->move(Vec2i(0, 1), true))
							{
								if(p_obj->flags & OF_BURSTABLE) p->burst();
							}
							else
							{
								position += dir;
								moved = true;
							}

							p->moved = false;
						}
						else if(!(p_obj->flags & OF_FIXED))
						{
							if(!p_obj->move(Vec2i(0, 1)))
							{
								if(p_obj->flags & OF_BURSTABLE) p_obj->burst();
							}
							else
							{
								position += dir;
								moved = true;
							}

							p_obj->moved = false;
						}
					}
				}
			}
		}
	}

	if(moved)
	{
		level.hashObject(this);
	}

	// Wurde das Objekt zur Seite bewegt?
	if(moved && dir.x && !dir.y)
	{
		// Befindet sich über diesem Objekt ein anderes (Stapel)?
		Object* p_obj = level.getFrontObjectAt(oldPosition - Vec2i(0, 1));
		if(p_obj)
		{
			if(p_obj->isPushedFromAbove())
			{
				// das Objekt ebenfalls verschieben
				p_obj->onConveyorBelt = max(onConveyorBelt, p_obj->onConveyorBelt);
				p_obj->move(dir, force);
			}
		}
	}

	// Ist dieses Objekt ein Aufzug?
	if(moved && (flags & OF_ELEVATOR))
	{
		// Alle Objekte darüber müssen mitbewegt werden.
		std::vector<Object*> objectsOnMe = level.getObjectsAt(position - dir);
		for(std::vector<Object*>::const_iterator i = objectsOnMe.begin(); i != objectsOnMe.end(); ++i)
		{
			if((*i)->depth < this->depth && ((*i)->flags & OF_TRANSPORTABLE))
			{
				(*i)->moved = false;
				if((*i)->getType() == "Player") static_cast<Player*>(*i)->move(dir);
				else
				{
					if(!(*i)->move(dir))
					{
						if((*i)->getFlags() & OF_FIXED)
						{
							// Umkehren!
							position -= dir;
							level.hashObject(this);
							moved = false;
						}
					}
				}

				(*i)->moved = false;
			}
		}
	}

	if(moved)
	{
		lastMoveDir = dir;

		if(dir.y > 0)
		{
			// Das Objekt fällt.
			fall = 1;
		}

		handleSliding();
	}
	else
	{
		slideDir = -1;

		if(fall == 1 && dir.y > 0)
		{
			// Das Objekt ist auf ein anderes Objekt gefallen.
			fall = -1;
		}
	}

	return moved;
}

void Object::handleSliding()
{
	if(slideDir == -2)
	{
		slideDir = -1;
		return;
	}

	if(!isPushedFromAbove())
	{
		if(!level.getElevatorAt(position))
		{
			// Sind wir auf Eis?
			uint l0 = level.getTileAt(0, position);
			const TileSet::TileInfo& t0 = level.getTileSet()->getTileInfo(l0);
			if(t0.type == 4)
			{
				// Ja!
				slideDir = dirToInt(lastMoveDir);
			}
			else slideDir = -1;
		}
		else
		{
			slideDir = -1;
		}
	}
	else slideDir = -1;
}

bool Object::allowMovement(const Vec2i& dir)
{
	return true;
}

bool Object::reflectLaser(Vec2i& dir,
						  bool lightBarrier)
{
	return false;
}

bool Object::reflectProjectile(Vec2d& velocity)
{
	return false;
}

void Object::onFire()
{
}

void Object::burst()
{
	// platzen
	Engine::inst().playSound(burstSound, false, 0.1, 100);
	ParticleSystem* p_particleSystem = level.getParticleSystem();
	ParticleSystem::Particle p;
	for(int i = 0; i < 75; i++)
	{
		p.lifetime = random(20, 50);
		p.damping = 0.85f;
		p.gravity = 0.1f;
		p.positionOnTexture = Vec2b(96, 0);
		p.sizeOnTexture = Vec2b(16, 16);
		p.position = position * 16 + Vec2i(8, 8) + Vec2i(random(-4, 4), random(-4, 4));
		const double r = random(0.0, 6.283);
		p.velocity = random(2.0, 5.0) * Vec2d(sin(r), cos(r));
		p.color = debrisColor + Vec4d(random(-0.1, 0.1), random(-0.1, 0.1), random(-0.1, 0.1), 0.5);
		p.deltaColor = Vec4d(0.0, 0.0, 0.0, -p.color.a / p.lifetime);
		p.rotation = random(0.0f, 10.0f);
		p.deltaRotation = random(-0.1f, 0.1f);
		p.size = random(0.5f, 0.8f);
		p.deltaSize = random(0.0f, -0.005f);
		p_particleSystem->addParticle(p);
	}

	disappear(0.2);
}

bool Object::changeInEditor(int mod)
{
	return false;
}

void Object::saveAttributes(TiXmlElement* p_target)
{
}

void Object::saveExtendedAttributes(TiXmlElement* p_target)
{
	char temp[256] = "";
	sprintf(temp, "%f", shownPosition.x);
	p_target->SetAttribute("shownPositionX", temp);
	sprintf(temp, "%f", shownPosition.y);
	p_target->SetAttribute("shownPositionY", temp);
}

void Object::loadExtendedAttributes(TiXmlElement* p_element)
{
	const char* p_temp;
	p_temp = p_element->Attribute("shownPositionX");
	if(p_temp) sscanf(p_temp, "%f", &shownPosition.x);
	p_temp = p_element->Attribute("shownPositionY");
	if(p_temp) sscanf(p_temp, "%f", &shownPosition.y);
}

void Object::frameBegin()
{
	moved = false;
}

void Object::disappear(double duration)
{
	if(duration == 0.0)
	{
		deathCountDown = -1.0;
		deathSpeed = 0.0;
	}
	else
	{
		deathCountDown = 0.9999;
		deathSpeed = 1.0 / duration;
	}
}

void Object::disappearNextFrame(double duration)
{
	if(duration == 0.0)
	{
		newDeathCountDown = -1.0;
		newDeathSpeed = 0.0;
	}
	else
	{
		newDeathCountDown = 0.9999;
		newDeathSpeed = 1.0 / duration;
	}

	newDeathTime = level.time + 1;
}

bool Object::isPushedFromAbove()
{
	if(moved) return false;
	if(flags & OF_GRAVITY) return true;
	if(flags & OF_FIXED) return false;

	for(int y = position.y - 1; y >= 0; y--)
	{
		Object* p_onTop = level.getFrontObjectAt(Vec2i(position.x, y));
		if(p_onTop)
		{
			if(p_onTop->hasMoved()) return false;
			else if(p_onTop->getFlags() & OF_GRAVITY) break;
			else if(p_onTop->getFlags() & OF_FIXED) return false;
		}
		else return false;
	}

	return true;
}

bool Object::isPushedWithDeadlyWeight()
{
	if(moved) return false;
	if(flags & OF_DEADLY_WEIGHT) return true;
	if(flags & OF_FIXED) return false;

	for(int y = position.y - 1; y >= 0; y--)
	{
		Object* p_onTop = level.getFrontObjectAt(Vec2i(position.x, y));
		if(p_onTop)
		{
			if(p_onTop->hasMoved()) return false;
			else if(p_onTop->getFlags() & OF_DEADLY_WEIGHT) return true;
			else if(p_onTop->getFlags() & OF_FIXED) return false;
		}
		else return false;
	}

	return false;
}

void Object::say(const std::string& text,
				 double duration)
{
	sayText = text;
	sayTime = duration;
}

const std::string& Object::getType() const
{
	return type;
}

void Object::setType(const std::string& type)
{
	this->type = type;
}

const Vec2i& Object::getPosition() const
{
	return position;
}

void Object::moveTo(const Vec2i& position)
{
	this->position = position;
	level.hashObject(this);
}

void Object::warpTo(const Vec2i& position)
{
	this->position = this->shownPosition = position;
	level.hashObject(this);
}

void Object::teleportTo(const Vec2i& position)
{
	if(teleporting != 0.0) return;

	teleporting = 0.02;
	teleportingTo = position;

	// Beim Teleportieren muss das Objekt vor allen anderen sein.
	oldDepth = depth;
	setDepth(-100);

	Engine::inst().playSound("teleport_begin.ogg", false, 0.0, 100);
}

bool Object::hasMoved() const
{
	return moved;
}

const Vec2d& Object::getRealShownPosition() const
{
	return shownPosition;
}

Vec2i Object::getShownPosition() const
{
	return Vec2d(0.5, 0.5) + shownPosition;
}

Vec2i Object::getShownPositionInPixels() const
{
	return Vec2d(0.5, 0.5) + shownPosition * 16.0;
}

uint Object::getFlags() const
{
	return flags;
}

void Object::setFlags(uint flags)
{
	this->flags = flags;
}

int Object::getDepth() const
{
	return depth;
}

void Object::setDepth(int depth)
{
	this->depth = depth;
}

bool Object::isGhost() const
{
	return ghost;
}

void Object::setGhost(bool ghost)
{
	this->ghost = ghost;
}

int Object::getDestroyTime() const
{
	return destroyTime;
}

void Object::setDestroyTime(int destroyTime)
{
	this->destroyTime = destroyTime;
}

bool Object::isAlive() const
{
	return deathCountDown == 1.0;
}

bool Object::toBeRemoved() const
{
	return deathCountDown <= 0.0;
}

void Object::setCollisionSound(const std::string& collisionSound)
{
	this->collisionSound = collisionSound;
}

bool Object::isTeleporting() const
{
	return teleporting != 0.0;
}

bool Object::hasTeleportFailed() const
{
	return teleportFailed;
}

bool Object::isFalling() const
{
	return falling > 0.0;
}

const Vec4d& Object::getDebrisColor() const
{
	return debrisColor;
}

void Object::setDebrisColor(const Vec4d& debrisColor)
{
	this->debrisColor = debrisColor;
}

uint Object::getMass() const
{
	return mass;
}

void Object::setMass(uint mass)
{
	this->mass = mass;
}

uint Object::getUID() const
{
	return uid;
}

void Object::setUID(uint uid)
{
	this->uid = uid;
}

std::string Object::getToolTip() const
{
	return toolTip;
}

void Object::setToolTip(const std::string& toolTip)
{
	this->toolTip = toolTip;
}

Vec4d Object::getStdColor(uint color)
{
	switch(color % 6)
	{
	case 0:
		return Vec4d(0.9, 0.9, 0.9, 1.0);
	case 1:
		return Vec4d(0.4, 0.9, 0.4, 1.0);
	case 2:
		return Vec4d(0.4, 0.4, 0.9, 1.0);
	case 3:
		return Vec4d(0.4, 0.9, 0.9, 1.0);
	case 4:
		return Vec4d(0.9, 0.9, 0.4, 1.0);
	case 5:
		return Vec4d(0.9, 0.4, 0.9, 1.0);
	}

	return Vec4d(0.0, 0.0, 0.0, 0.0);
}

Vec2i Object::intToDir(int dir)
{
	switch(dir % 4)
	{
	case 0: return Vec2i(0, -1);
	case 1: return Vec2i(1, 0);
	case 2: return Vec2i(0, 1);
	case 3: return Vec2i(-1, 0);
	default: return Vec2i(0, 0);
	}
}

int Object::dirToInt(const Vec2i& dir)
{
	Vec2i a(abs(dir.x), abs(dir.y));
	if(a.x >= a.y) return dir.x < 0 ? 3 : 1;
	else return dir.y < 0 ? 0 : 2;
}