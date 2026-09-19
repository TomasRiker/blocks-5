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

// How brightly an object that has called flash() lights up, and how fast that
// dies away again. The decay is that of the flash in Level::update(): a
// fifth less per logic tick, off below 1/256 - 25 ticks, half a second. Both
// are a matter of taste, which is why they live here. extern, because the
// HUD icons light up with the same numbers.
extern const float FLASH_STRENGTH = 1.0f;
extern const float FLASH_DECAY = 0.8f;

// Where a beam is drawn, against where its emitter traced it. Laser and
// LightBarrierSender both start at getShownPositionInPixels() + 7.5, the
// centre of pixel 7 - but a 16-pixel cell has its centre on the boundary
// between pixels 7 and 8, and that is the line the art is drawn about: the
// laser's ruby has its two strong columns at 7 and 8, and the light barrier's
// lens is symmetric to the pixel about the same boundary, in every shipped
// skin. A quad is rasterised by whether a pixel's centre lies inside it, so a
// line centred half a pixel to the left of that boundary lights column 7 and
// not column 8 - which is why the beam looked as if it left the emitter beside
// its ruby rather than out of it.
//
// The traced points cannot move: the hit test reads them. So the drawing adds
// this, and once a beam is centred on the boundary every cross-width is an
// even number of pixels. That is not tidiness: at that centre an odd width
// puts its two edges exactly on two pixel centres, where the fill rule decides
// what is lit, and a width below one pixel passes between two centres and
// draws nothing at all.
extern const float BEAM_DRAW_OFFSET = 0.5f;

// What is left to see of a block at the end of the conversion. Not 0: it is
// solid until the last tick, and an invisible obstacle would be a bug and not
// an effect.
static const float CONVERSION_GHOST = 0.22f;

int Object::nextFallingDepth = 1000000;

Object::Object(Level& level,
			   int depth) : level(level), depth(depth)
{
	level.addObject(this);
	position = shownPosition = Vec2i(0, 0);
	noCollect = 0.0f;
	flags = 0;
	ghost = false;
	destroyTime = 0;
	deathCountDown = 1.0f;
	glowJitter = 0.0f;
	conversionProgress = 0.0f;
	deathSpeed = 0.0f;
	newDeathCountDown = 1.0f;
	newDeathSpeed = 0.0f;
	newDeathTime = -1;
	interpolation = 0.3f;
	collisionSound = "";
	moved = false;
	lastMoveDir = Vec2i(0, 0);
	teleporting = 0.0f;
	teleportFailed = false;
	oldDepth = depth;
	falling = 0.0f;
	mass = 0;
	uid = 0;
	fall = 0;
	lastHashedAt = -1;
	removed = false;
	flashAmount = 0.0f;
	flashLayer = RL_MAIN;
	renderLayers = 0;
	sayText = "";
	sayTime = 0.0f;
	sayAlpha = 0.0f;
	shadowPass = false;
	slideDir = -1;
	slideMove = false;
	onConveyorBelt = 0;
}

Object::~Object()
{
}

void Object::render(RenderLayer layer,
					const Vec2i& offset,
					const Vec4f& color)
{
	// The block of a running conversion goes pale. CONVERSION_GHOST is what is
	// left of it: it must not disappear entirely, because it can be pushed
	// right up to the last moment.
	const float converting = 1.0f - conversionProgress * (1.0f - CONVERSION_GHOST);
	Vec4f realColor(color.r, color.g, color.b, color.a * deathCountDown * converting);

	Renderer& renderer = Renderer::inst();
	renderer.push();

	if(!(flags & OF_PROXY))
	{
		if(layer == RL_WIRE) renderer.translate(static_cast<float>(offset.x), static_cast<float>(offset.y));
		else
		{
			Vec2i sp = getShownPositionInPixels() + offset;
			renderer.translate(static_cast<float>(sp.x), static_cast<float>(sp.y));
		}

		if(layer != RL_LIGHT)
		{
			float o = -16.0f;
			if(getType() == "Enemy") o = -17.0f;

			if(teleporting > 0.0f)
			{
				float y = 1.0f + teleporting * teleporting * 500.0f;
				renderer.translate(0.0f, y * o + 16.0f);
				renderer.scale(1.0f, y);
				realColor *= 1.0f - teleporting;
			}
			else if(teleporting < 0.0f)
			{
				float y = 1.0f + teleporting * teleporting * 500.0f;
				renderer.translate(0.0f, y * o + 16.0f);
				renderer.scale(1.0f, y);
				realColor *= 1.0f + teleporting;
			}
			else if(falling > 0.0f)
			{
				float f = 1.0f / (1.0f + 4.0f * falling);
				float size = 16.0f * f;
				float add = (16.0f - size) * 0.5f;
				renderer.translate(add, add);
				renderer.scale(f, f);
				renderer.rotate(falling * 120.0f);
				realColor.r *= f;
				realColor.g *= f;
				realColor.b *= f;
			}
		}
	}

	onRender(layer, realColor);

	// The flash, additive. The sprite colour could not carry it: for five of
	// the seven switches that is the default white, and the vertex stage
	// clamps a colour at 1 - there is nothing brighter than white in a colour
	// value. Added on
	// top there is, and the amount still comes from the sprite's colour,
	// letting a tinted switch light up in its own colour.
	//
	// Only on the layer the object has just drawn its sprite on, and not in
	// the shadow pass: that one goes through with black RGB, and an additive
	// pass would turn it into a bright spot in the middle of the shadow.
	if(flashAmount > 0.0f && layer == flashLayer && !shadowPass)
	{
		renderer.setBlend(BM_ADDITIVE);
		Engine::inst().renderSprites(sprites, Vec4f(flashAmount, flashAmount, flashAmount, realColor.a));
		renderer.setBlend(BM_NORMAL);
	}

	if(layer == RL_OVERLAY &&
	   sayTime > 0.0f &&
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

		renderer.push();

		renderer.translate(8.0f, 8.0f);
		if(mirrorX == -1) renderer.scale(-1.0f, 1.0f);
		if(mirrorY == -1) renderer.scale(1.0f, -1.0f);
		renderer.translate(8.0f, 8.0f);

		// The tail is a triangle: a quad with its last corner doubled draws
		// it and a second triangle of no area.
		const float top = 0.85f * sayAlpha;
		const float bottom = 0.6f * sayAlpha;
		const Vec4f fill(0.75f, 0.75f, 1.0f, top);
		const float right = static_cast<float>(15 + dim.x - 25);
		const float low = static_cast<float>(15 + dim.y);
		const Vec2f tail[4] = {Vec2f(2.0f, 2.0f), Vec2f(15.0f, 15.0f), Vec2f(5.0f, 15.0f), Vec2f(5.0f, 15.0f)};
		const Vec2f flat[4] = {Vec2f(8.5f, 8.5f), Vec2f(8.5f, 8.5f), Vec2f(8.5f, 8.5f), Vec2f(8.5f, 8.5f)};
		renderer.quad(renderer.state(), tail, flat, fill);
		const Vec2f body[4] = {Vec2f(-10.0f, 15.0f), Vec2f(right, 15.0f), Vec2f(right, low), Vec2f(-10.0f, low)};
		const Vec4f bodyColors[4] = {fill, fill, Vec4f(0.75f, 0.75f, 1.0f, bottom), Vec4f(0.75f, 0.75f, 1.0f, bottom)};
		renderer.quad(renderer.state(), body, flat, bodyColors);

		std::vector<Vec2f> outline;
		outline.push_back(Vec2f(2.0f, 2.0f));
		outline.push_back(Vec2f(15.0f, 15.0f));
		outline.push_back(Vec2f(right, 15.0f));
		outline.push_back(Vec2f(right, low));
		outline.push_back(Vec2f(-10.0f, low));
		outline.push_back(Vec2f(-10.0f, 15.0f));
		outline.push_back(Vec2f(5.0f, 15.0f));
		renderer.polyline(outline, 1.0f, Vec4f(0.0f, 0.0f, 0.0f, 0.9f * sayAlpha), true);

		renderer.pop();

		Vec2i textPosition(3 * mirrorX, 28 * mirrorY);
		if(mirrorX == -1) textPosition.x -= dim.x - 10;
		if(mirrorY == -1) textPosition.y -= dim.y - 10;
		p_font->renderText(str, Vec2i(8, 7) + textPosition, Vec4f(1.0f, 1.0f, 1.0f, sayAlpha));

		p_font->popOptions();
	}

	renderer.pop();
}

void Object::update()
{
	if(noCollect > 0.0f)
	{
		noCollect -= 0.02f;
		if(noCollect < 0.0f) noCollect = 0.0f;
	}

	if(sayTime > 0.0f)
	{
		if(sayTime < 0.25f) sayAlpha = sayTime * 4.0f;
		else
		{
			sayAlpha += 4.0f * 0.02f;
			sayAlpha = min(sayAlpha, 1.0f);
		}

		sayTime -= 0.02f;
		if(sayTime < 0.0f)
		{
			sayText = "";
			sayTime = 0.0f;
			sayAlpha = 0.0f;
		}
	}

	if((flags & OF_PROXY))
	{
		// update the object
		onUpdate();

		// run the death countdown down
		deathCountDown -= deathSpeed * 0.02f;
		return;
	}

	if(isAlive() &&
	   teleporting == 0.0f &&
	   falling == 0.0f)
	{
		if(fall == -1)
		{
			fall = 0;

			// play the collision sound
			Engine::inst().playSound(collisionSound, false, 0.15f, -100);

			// The object has collided with another object. Which one is it?
			Object* p_obj = level.getFrontObjectAt(position + Vec2i(0, 1));
			if(p_obj)
			{
				if(flags & OF_DEADLY)
				{
					// Can the object burst?
					if(p_obj->flags & OF_BURSTABLE)
					{
						// Yes - then let it do that now.
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
				// gravity
				move(Vec2i(0, 1));
			}

			if(slideDir != -1)
			{
				// sliding
				slideMove = true;
				if(type == "Player") static_cast<Player*>(this)->move(intToDir(slideDir));
				else move(intToDir(slideDir), mass);
				slideMove = false;
			}
		}

		// Is the object collectable, and is a player here?
		if(flags & OF_COLLECTABLE && noCollect == 0.0f)
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

		// update the object
		onUpdate();
	}

	// update the shown position
	float i;
	if(level.isElectricityOn() && level.getElevatorAt(position)) i = 0.12f;
	else i = interpolation;

	if(onConveyorBelt)
	{
		i = 0.12f;
		onConveyorBelt--;
	}

	if(i == -1.0f)
	{
		Vec2f dir = Vec2f(position) - shownPosition;
		float l = dir.length();
		float stepSize = 0.02f * (50.0f / 15.0f);

		if(l >= stepSize)
		{
			dir /= l;
			shownPosition += stepSize * dir;
		}
	}
	else
	{
		shownPosition = shownPosition * (1.0f - i) + Vec2f(position) * i;
	}

	// run the death countdown down
	if(newDeathTime != -1 && level.time >= newDeathTime)
	{
		deathCountDown = newDeathCountDown;
		deathSpeed = newDeathSpeed;
		newDeathTime = -1;
	}

	deathCountDown -= deathSpeed * 0.02f;

	if(teleporting != 0.0f)
	{
		ParticleSystem* p_particleSystem = level.getParticleSystem();
		ParticleSystem::Particle p;

		float x;
		if(teleporting > 0.0f) x = teleporting;
		else x = teleporting + 1.0f;

		if(x <= 0.5f)
		{
			float r = 7.0f + 15.0f * x;
			Vec2f o(r * sinf(x * 18.0f), -r * cosf(x * 18.0f));

			p.lifetime = 60;
			p.damping = 0.9f;
			p.gravity = 0.0f;
			p.positionOnTexture = Vec2b(0, 0);
			p.sizeOnTexture = Vec2b(16, 16);
			p.position = position * 16 + Vec2i(8, 8) + o;
			p.velocity = o * 0.01f;
			p.color = Vec4f(0.1f, 0.1f, 1.0f, 0.6f);
			p.deltaColor = Vec4f(2.0f, 1.0f, -2.0f, 0.4f) / p.lifetime;
			p.rotation = random(0.0f, 10.0f);
			p.deltaRotation = random(-0.1f, 0.1f);
			p.size = random(0.5f, 0.75f);
			p.deltaSize = -p.size / p.lifetime;
			p_particleSystem->addParticle(p);
		}

		// teleport
		if(teleporting > 0.0f)
		{
			teleporting += 0.02f;
			if(teleporting > 1.0f)
			{
				// change the position
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
					Engine::inst().playSound("teleport_failed.ogg", false, 0.0f, 100);

					// glowing particles
					for(int i = 0; i < 100; i++)
					{
						p.lifetime = 100;
						p.damping = 0.9f;
						p.gravity = 0.05f;
						p.positionOnTexture = Vec2b(32, 32);
						p.sizeOnTexture = Vec2b(16, 16);
						p.position = teleportingTo * 16 + Vec2i(random(6, 10), random(6, 10));
						const float r = random(0.0f, 6.283f);
						p.velocity = random(3.0f, 6.0f) * Vec2f(sinf(r), cosf(r));
						p.color = Vec4f(random(0.5f, 1.0f), random(0.5f, 1.0f), 0.0f, 0.9f);
						p.deltaColor = Vec4f(0.5f, 0.0f, 0.0f, -p.color.a / p.lifetime);
						p.rotation = random(0.0f, 10.0f);
						p.deltaRotation = random(-0.1f, 0.1f);
						p.size = random(0.15f, 0.25f);
						p.deltaSize = random(-0.01f, -0.005f);
						p_particleSystem->addParticle(p);
					}
				}

				teleporting = -1.0f;
				Engine::inst().playSound("teleport_end.ogg", false, 0.0f, 100);
			}
		}
		else if(teleporting < 0.0f)
		{
			teleporting += 0.02f;
			if(teleporting > 0.0f)
			{
				teleporting = 0.0f;
				setDepth(oldDepth);
			}
		}
	}

	if(teleporting == 0.0f && falling == 0.0f && isAlive() && !ghost)
	{
		// Is there a hole below us?
		uint tileID = level.getTileAt(0, position);
		const TileSet::TileInfo& tileInfo = level.getTileSet()->getTileInfo(tileID);
		if(tileInfo.type == 3)
		{
			bool dontFall = false;

			if(flags & OF_DONT_FALL) dontFall = true;

			// Rails do not fall.
			else if(flags & OF_RAIL) dontFall = true;

			// Objects on elevators do not fall.
			else if(!(flags & OF_ELEVATOR) && (flags & OF_TRANSPORTABLE) && level.getElevatorAt(position)) dontFall = true;

			// Elevators on rails do not fall.
			else if(flags & OF_ELEVATOR)
			{
				Object* p_back = level.getBackObjectAt(position);
				if(p_back)
				{
					if(p_back->flags & OF_RAIL) dontFall = true;
				}
			}

			// Objects that just missed an elevator get another chance.
			else if(flags & OF_TRANSPORTABLE)
			{
				// search the 4 surrounding fields for elevators
				std::list<Elevator*> elevators;
				Elevator* p_elevator = level.getElevatorAt(position + Vec2i(-1, 0)); if(p_elevator) elevators.push_back(p_elevator);
				p_elevator = level.getElevatorAt(position + Vec2i(1, 0)); if(p_elevator) elevators.push_back(p_elevator);
				p_elevator = level.getElevatorAt(position + Vec2i(0, -1)); if(p_elevator) elevators.push_back(p_elevator);
				p_elevator = level.getElevatorAt(position + Vec2i(0, 1)); if(p_elevator) elevators.push_back(p_elevator);

				// work out the distances
				Elevator* p_closestElevator = 0;
				float closestDist = 0.0f;
				for(std::list<Elevator*>::const_iterator i = elevators.begin(); i != elevators.end(); ++i)
				{
					if((*i)->getPosition() != position - lastMoveDir)
					{
						float dist = ((*i)->shownPosition - position).lengthSq();
						if(!p_closestElevator || dist < closestDist) p_closestElevator = *i, closestDist = dist;
					}
				}

				if(p_closestElevator)
				{
					// Is the nearest elevator close enough?
					if(closestDist <= 0.5f)
					{
						Vec2i d = p_closestElevator->getPosition() - position;
						Vec2i d1, d2;
						if(lastMoveDir.x) d1 = Vec2i(0, d.y), d2 = Vec2i(d.x, 0);
						else d1 = Vec2i(d.x, 0), d2 = Vec2i(0, d.y);

						// push the object onto the elevator
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
				// Yes, fall!
				Engine::inst().playSound("falling.ogg", false, 0.0f, 100);
				falling = 0.0001f;
				ghost = true;
				setDepth(nextFallingDepth--);
			}
		}
	}
	else if(falling > 0.0f)
	{
		falling += 0.02f;
		if(falling > 1.0f && isAlive()) disappear(0.2f);
	}
}

void Object::onRemove()
{
}

void Object::onRender(RenderLayer layer,
					  const Vec4f& color)
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
	disappear(0.2f);
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
		// destroy the object
		disappear(0.2f);
	}
}

// simulate takes every decision this function takes and performs none of
// them: each path returns at the point where it would otherwise commit, and
// the push recurses in simulate mode too, so the answer covers a whole chain
// of pushable objects against a wall. A mouse drag asks it before it holds a
// direction key down, because the alternative - hold the key and see whether
// anything moved - is not free: a walk into a switch or a magnet *works* it,
// and the character would flip whatever it brushed while the drag routed
// around it.
//
// Two of the early-outs are skipped when asking, and that is deliberate.
// Whether the object has already moved in this tick, and whether it is
// sliding, decide *when* a step lands rather than whether the way is open;
// the caller holds its key across ticks, and an answer that flickered with
// the tick would hand its leg to the other axis and back. A push onto ice is
// the one place the answer is generous: the pushed object starts sliding
// instead of stepping, so the real move() reports false while the way is in
// fact opening.
bool Object::move(const Vec2i& dir,
				  uint force,
				  bool simulate)
{
	if(!simulate && slideDir != -1 && !slideMove) return false;

	if(dir.isZero()) return true;
	if(force < mass || teleporting != 0.0f) return false;
	if(!simulate && moved) return false;
	if(!level.isValidPosition(position + dir)) return false;

	if(dir.x && !dir.y && isPushedFromAbove() && level.isElectricityOn())
	{
		// Is the object lying on a conveyor belt?
		Object* p_obj = level.getFrontObjectAt(position + Vec2i(0, 1));
		if(p_obj)
		{
			if(p_obj->getType() == "ConveyorBelt")
			{
				ConveyorBelt* p_cb = reinterpret_cast<ConveyorBelt*>(p_obj);
				if(p_cb->getDir() != dir.x)
				{
					// Pushing against the conveyor belt's direction is not allowed!
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
		if(simulate) return true;
		position = np;
		moved = true;
	}
	else if(tileType != 1 && tileType != 2)
	{
		Object* p_obj = level.getFrontObjectAt(position + dir);
		if(p_obj)
		{
			// Are we a collectable object and the other object the player?
			if((flags & OF_COLLECTABLE) && p_obj->getType() == "Player")
			{
				// Then it is OK.
				if(simulate) return true;
				position += dir;
				moved = true;
			}
			// Are we an elevator and the other object transportable?
			else if((flags & OF_ELEVATOR) && (p_obj->getFlags() & OF_TRANSPORTABLE))
			{
				// Then it is OK.
				if(simulate) return true;
				position += dir;
				moved = true;
			}
			// Can this object move?
			else if(!(p_obj->getFlags() & OF_FIXED) && !p_obj->isPushedWithDeadlyWeight())
			{
				if(p_obj->isPushedFromAbove() && dir.y < 0)
				{
					// Pushing upward does not work while the object is being pushed down!
				}
				else
				{
					// The push, and the whole chain behind it: whatever stands
					// there is asked the same question with what is left of the
					// force.
					if(simulate) return p_obj->move(dir, force - mass, true);

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
				// Is the object being pushed with extreme force?
				if(isPushedWithDeadlyWeight())
				{
					// Is there an object below the "deadly" one?
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

	// Nothing above said yes, and everything below here is what a move does -
	// or does instead of moving, which a question must not do either.
	if(simulate) return false;

	if(moved)
	{
		level.hashObject(this);
	}

	// Has the object been moved sideways?
	if(moved && dir.x && !dir.y)
	{
		// Is there another object above this one (a stack)?
		Object* p_obj = level.getFrontObjectAt(oldPosition - Vec2i(0, 1));
		if(p_obj)
		{
			if(p_obj->isPushedFromAbove())
			{
				// move that object too
				p_obj->onConveyorBelt = max(onConveyorBelt, p_obj->onConveyorBelt);
				p_obj->move(dir, force);
			}
		}
	}

	// Is this object an elevator?
	if(moved && (flags & OF_ELEVATOR))
	{
		// Every object above it has to move along with it.
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
							// Reverse!
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
			// The object is falling.
			fall = 1;
		}

		handleSliding();
	}
	else
	{
		slideDir = -1;

		if(fall == 1 && dir.y > 0)
		{
			// The object has fallen onto another object.
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
			// Are we on ice?
			uint l0 = level.getTileAt(0, position);
			const TileSet::TileInfo& t0 = level.getTileSet()->getTileInfo(l0);
			if(t0.type == 4)
			{
				// Yes!
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

bool Object::reflectProjectile(Vec2f& velocity)
{
	return false;
}

void Object::onFire()
{
}

void Object::burst()
{
	// burst
	Engine::inst().playSound(burstSound, false, 0.1f, 100);
	ParticleSystem* p_particleSystem = level.getParticleSystem();
	ParticleSystem::Particle p;
	const Sprites& debris = getSprites();
	const int numTries = debris.getTryCount(75);
	for(int i = 0; i < numTries; i++)
	{
		p.lifetime = static_cast<ushort>(random(20, 50));
		p.damping = 0.85f;
		p.gravity = 0.1f;
		p.positionOnTexture = Vec2b(96, 0);
		p.sizeOnTexture = Vec2b(16, 16);
		// Take the colour and the spot from the image. If the try lands on a
		// transparent spot, no particle is created - a small object therefore
		// throws less debris than a big one, without that having to be set
		// anywhere. The spot doubles as the starting point, which keeps the
		// cloud in the object's shape.
		Vec4f sampled;
		Vec2i offset;
		if(!debris.sample(&sampled, &offset)) continue;

		p.position = position * 16 + offset;
		const float r = random(0.0f, 6.283f);
		p.velocity = random(2.0f, 5.0f) * Vec2f(sinf(r), cosf(r));
		p.color = sampled + Vec4f(0.0f, 0.0f, 0.0f, 0.5f);
		p.deltaColor = Vec4f(0.0f, 0.0f, 0.0f, -p.color.a / p.lifetime);
		p.rotation = random(0.0f, 10.0f);
		p.deltaRotation = random(-0.1f, 0.1f);
		p.size = random(0.5f, 0.8f);
		p.deltaSize = random(0.0f, -0.005f);
		p_particleSystem->addParticle(p);
	}

	disappear(0.2f);
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
	// QueryFloatAttribute and not Attribute() with sscanf: it leaves the value
	// alone where the attribute is missing, and there the shown position is
	// already the grid position the preset warped the object to - Attribute()
	// would hand a null pointer straight into sscanf instead.
	p_element->QueryFloatAttribute("shownPositionX", &shownPosition.x);
	p_element->QueryFloatAttribute("shownPositionY", &shownPosition.y);
}

void Object::frameBegin()
{
	moved = false;

	// See setConversionProgress(): clearing it and letting the machine set it
	// again straight away is the undo nobody can forget. No longer once the
	// block is dying: then nobody is pushing any more, and at the moment of
	// the conversion the ghosted block would abruptly stand there as a full
	// block again over the diamond the sparks have just built.
	//
	// newDeathTime has to be asked as well, and the whole case hangs on that:
	// disappearNextFrame() only records the death, which takes effect in
	// update() - that is, after this frameBegin(). isAlive() alone still says
	// "alive" in the tick after, and that is exactly where it would be
	// cleared.
	if(isAlive() && newDeathTime == -1) conversionProgress = 0.0f;

	// The decay belongs here and not in onBeforeRender(): that runs per frame,
	// and the flash would otherwise hang off the frame rate.
	if(flashAmount > 0.0f)
	{
		flashAmount *= FLASH_DECAY;
		if(flashAmount < 1.0f / 256.0f) flashAmount = 0.0f;
	}

	// And the glow's unsteadiness for the same reason - see glowJitter. Drawn
	// for every object rather than only for the ones that glow, because a
	// draw from the shared generator has to happen the same number of times
	// whatever is on screen, or a frame stops being reproducible from a seed.
	glowJitter = random(-1.0f, 1.0f);
}

void Object::flash()
{
	flashAmount = FLASH_STRENGTH;

	// render() puts the flash on flashLayer, which is not necessarily a layer
	// this object's own sprites reach. The bit goes in here and never comes
	// out: a mask may name a layer the object is not drawing on this frame,
	// and may never omit one it is.
	renderLayers |= flashLayer;
}

void Object::disappear(float duration)
{
	if(duration == 0.0f)
	{
		deathCountDown = -1.0f;
		deathSpeed = 0.0f;
	}
	else
	{
		deathCountDown = 0.9999f;
		deathSpeed = 1.0f / duration;
	}
}

void Object::disappearNextFrame(float duration)
{
	if(duration == 0.0f)
	{
		newDeathCountDown = -1.0f;
		newDeathSpeed = 0.0f;
	}
	else
	{
		newDeathCountDown = 0.9999f;
		newDeathSpeed = 1.0f / duration;
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
				 float duration)
{
	sayText = text;
	sayTime = duration;

	// The balloon is render()'s own, not onRender()'s, so the class that set
	// the mask in its constructor knows nothing about it. As in flash(), the
	// bit is added and never removed.
	renderLayers |= RL_OVERLAY;
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
	if(teleporting != 0.0f) return;

	teleporting = 0.02f;
	teleportingTo = position;

	// While teleporting the object has to be in front of all the others.
	oldDepth = depth;
	setDepth(-100);

	Engine::inst().playSound("teleport_begin.ogg", false, 0.0f, 100);
}

bool Object::hasMoved() const
{
	return moved;
}

const Vec2f& Object::getRealShownPosition() const
{
	return shownPosition;
}

Vec2i Object::getShownPosition() const
{
	return Vec2f(0.5f, 0.5f) + shownPosition;
}

Vec2i Object::getShownPositionInPixels() const
{
	return Vec2f(0.5f, 0.5f) + shownPosition * 16.0f;
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
	return deathCountDown == 1.0f;
}

bool Object::toBeRemoved() const
{
	return deathCountDown <= 0.0f;
}

void Object::setCollisionSound(const std::string& collisionSound)
{
	this->collisionSound = collisionSound;
}

bool Object::isTeleporting() const
{
	return teleporting != 0.0f;
}

bool Object::hasTeleportFailed() const
{
	return teleportFailed;
}

bool Object::isFalling() const
{
	return falling > 0.0f;
}

void Object::onBeforeRender()
{
	rebuildSprites();
}

void Object::updateSprites()
{
}

const Sprites& Object::getSprites()
{
	rebuildSprites();
	return sprites;
}

void Object::rebuildSprites()
{
	sprites.clear();
	sprites.setTexture(level.getSpritesTexture());
	updateSprites();
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

Vec4f Object::getStdColor(uint color)
{
	switch(color % 6)
	{
	case 0:
		return Vec4f(0.9f, 0.9f, 0.9f, 1.0f);
	case 1:
		return Vec4f(0.4f, 0.9f, 0.4f, 1.0f);
	case 2:
		return Vec4f(0.4f, 0.4f, 0.9f, 1.0f);
	case 3:
		return Vec4f(0.4f, 0.9f, 0.9f, 1.0f);
	case 4:
		return Vec4f(0.9f, 0.9f, 0.4f, 1.0f);
	case 5:
		return Vec4f(0.9f, 0.4f, 0.9f, 1.0f);
	}

	return Vec4f(0.0f, 0.0f, 0.0f, 0.0f);
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