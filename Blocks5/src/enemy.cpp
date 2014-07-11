#include "pch.h"
#include "enemy.h"
#include "player.h"
#include "engine.h"
#include "tileset.h"
#include "particlesystem.h"

Enemy::Enemy(Level& level,
			 const Vec2i& position,
			 int subType,
			 int dir) : Object(level, 0)
{
	warpTo(position);
	setMass(1);
	this->subType = subType;
	this->dir = dir;
	shownDir = dir;
	anim = 0;
	moveCounter = 40;
	thinkCounter = 40;
	soundCounter = 40;
	eatCounter = 40;
	burpCounter = 0;
	targetPosition = Vec2i(-1, -1);
	interest = 0;
	contamination = 100;
	height = 0.0;
	vy = 0.0;
	invisibility = 0;

	if(subType == 0)
	{
		flags = OF_MASSIVE | OF_COLLECTABLE | OF_DESTROYABLE | OF_TRANSPORTABLE | OF_BURSTABLE;
		destroyTime = 1;
		positionOnTexture = Vec2i(0, 416);
	}
	else if(subType == 1)
	{
		flags = OF_MASSIVE | OF_COLLECTABLE | OF_DESTROYABLE | OF_TRANSPORTABLE | OF_BURSTABLE | OF_TRIGGER_PANELS;
		destroyTime = 1;
		positionOnTexture = Vec2i(64, 448);
	}

	burstSound = "enemy_burst.ogg";
}

Enemy::~Enemy()
{
}

void Enemy::onRender(int layer,
					 const Vec4d& color)
{
	Vec4d realColor = color;

	if(invisibility)
	{
		realColor.a -= 0.1 * invisibility;
	}

	if(layer == 1)
	{
		if(subType == 0)
		{
			// komisches grünes Insekt rendern
			int f[] = {0, 1, 0, 2};
			int frame = f[(anim / 4) % 4];
			double a = 10.0 * sin(anim / 4.0);
			Engine::inst().renderSprite(Vec2i(0, 0), Vec2i(frame * 32, 416), Vec2i(16, 16), realColor, false, 90.0 * shownDir + a);
		}
		else if(subType == 1)
		{
			// Teufelsfratze rendern
			int f[] = {0, 1, 1, 1, 0};
			int frame = f[(anim / 4) % 5];
			if(shadowPass) realColor.a /= 1.0 + 0.25 * height;
			Engine::inst().renderSprite(Vec2i(0, shadowPass ? 0 : static_cast<int>(-height)), Vec2i(64 + frame * 32, 448), Vec2i(16, 16), realColor);
		}
	}
	else if(layer == 18)
	{
		if(subType == 1)
		{
			if(interest >= 10000) level.renderShine(0.6, 0.4 + random(-0.05, 0.05));
		}
	}
}

void Enemy::onUpdate()
{
	if(invisibility) invisibility--;

	Vec2i facing = intToDir(dir);

	if(level.getAIFlags(position) & 2)
	{
		// Vergiftung
		contamination--;
	}

	if(contamination <= 0) burst();

	if(!thinkCounter--)
	{
		// den nächsten Spieler suchen, der für den Gegner sichtbar ist
		Player* p_closestPlayer = 0;
		int closestDist = 0;
		const std::list<Player*>& players = Player::getInstances();
		for(std::list<Player*>::const_iterator i = players.begin(); i != players.end(); ++i)
		{
			if(!(*i)->isTeleporting())
			{
				int dist = (position - (*i)->getPosition()).lengthSq();
				if(dist < closestDist || !p_closestPlayer)
				{
					if(canSee((*i)->getPosition()))
					{
						p_closestPlayer = *i;
						closestDist = dist;
					}
				}
			}
		}

		if(p_closestPlayer)
		{
			targetPosition = p_closestPlayer->getPosition();
			interest += 2225 - closestDist;
		}

		thinkCounter = random(2, 5);
	}

	if(subType == 0)
	{
		int oldDir = dir;

		if(moveCounter-- <= 0 && fabs(shownDir - dir) < 0.4)
		{
			int r = random(0, 8);
			if(interest >= 10000) r = random(0, 40);

			if(contamination < 50)
			{
				if(random() % 2) r = 0;
			}

			interest /= 2;

			switch(r)
			{
			case 0:
				{
					int d = random(-22, 22) / 10;
					if(d) dir += d, anim += 16;
				}
				break;
			case 1:
			case 2:
			case 3:
				if(!tryToMove(facing)) dir += random(-22, 22) / 10;
				anim += 16;
				break;
			default:
				if(r >= 6 && targetPosition.x != -1)
				{
					// zum Ziel laufen
					Vec2i toTarget = targetPosition - position;
					if(toTarget.x && toTarget.y) toTarget.value[random(0, 1)] = 0;
					int d = dirToInt(toTarget);
					toTarget = intToDir(d);
					if(tryToMove(toTarget))
					{
						dir = d;
						anim += 16;
						interest *= 2;
					}
				}
				break;
			}

			if(position == targetPosition || interest < 10)
			{
				targetPosition = Vec2i(-1, -1);
				interest = 0;
			}

			while(dir < 0) dir += 4, shownDir += 4.0;
			dir %= 4;

			moveCounter = random(4, 7);
		}

		if(anim) anim--;

		double dd = static_cast<double>(dir) - shownDir;
		if(dd > 2.0) shownDir += 4.0;
		else if(dd < -2.0) shownDir -= 4.0;
		shownDir = 0.125 * dir + 0.875 * shownDir;

		if(!soundCounter)
		{
			if(oldDir != dir)
			{
				// Kratz-Sound abspielen
				Engine::inst().playSound("enemy1_turn.ogg", false, 0.1, -100);
				soundCounter = random(15, 55);
			}
		}
		else soundCounter--;

		if(burpCounter)
		{
			burpCounter--;
			if(!burpCounter)
			{
				int s = random(0, 1);
				std::string sound;
				if(s == 0) sound = "enemy1_burp1.ogg";
				else if(s == 1) sound = "enemy1_burp2.ogg";
				Engine::inst().playSound(sound, false, 0.1);

				// Rülpspartikel erzeugen
				ParticleSystem* p_particleSystem = level.getParticleSystem();
				ParticleSystem::Particle p;
				for(int i = 0; i < 10; i++)
				{
					p.lifetime = random(50, 75);
					p.damping = 0.99f;
					p.gravity = random(-0.005f, -0.02f);
					p.positionOnTexture = Vec2b(96, 32);
					p.sizeOnTexture = Vec2b(16, 16);
					p.position = position * 16 + Vec2i(8 + random(-4, 4), 6);
					p.velocity = Vec2d(random(-0.5, 0.5), random(-1.0, -0.5));
					p.color = Vec4d(random(0.8, 1.0), random(0.8, 1.0), random(0.8, 1.0), 0.25);
					p.deltaColor = Vec4d(0.0, 0.0, 0.0, -p.color.a / p.lifetime);
					p.rotation = random(-0.5f, 0.5f);
					p.deltaRotation = random(-0.05f, 0.05f);
					p.size = random(0.2f, 1.0f);
					p.deltaSize = random(0.0f, 0.01f);
					p_particleSystem->addParticle(p);
				}
			}
		}
	}
	else if(subType == 1)
	{
		if(random() % 2) anim++;

		if(moveCounter-- <= 0)
		{
			int r = random(0, 1);
			if(interest > 6000) r = random(0, 50);

			if(contamination < 50)
			{
				if(random() % 2) r = 0;
			}

			interest /= 2;

			switch(r)
			{
			case 0:
				tryToMove(intToDir(random(0, 4)));
				break;
			default:
				if(targetPosition.x != -1)
				{
					// zum Ziel laufen
					Vec2i toTarget = targetPosition - position;
					if(toTarget.x && toTarget.y) toTarget.value[random(0, 1)] = 0;
					int d = dirToInt(toTarget);
					toTarget = intToDir(d);
					if(tryToMove(toTarget)) interest *= 2;
				}
				else
				{
					// Wo ist die Spur am heißesten?
					int bestDir = 0;
					uint bestTrace = 0;
					for(int dir = 0; dir < 4; dir++)
					{
						uint trace = level.getAITrace(position + intToDir(dir));
						if(trace > bestTrace)
						{
							bestTrace = trace;
							bestDir = dir;
						}
					}

					if(bestTrace)
					{
						Vec2i bestDirV = intToDir(bestDir);
						if(!tryToMove(bestDirV))
						{
							// Das ging nicht. Ist da ein anderer Gegner?
							bool reduceTrace = true;
							Object* p_obj = level.getFrontObjectAt(position + bestDirV);
							if(p_obj)
							{
								// Wenn da ein anderer Gegner ist, ist es egal.
								if(p_obj->getType() == "Enemy") reduceTrace = false;
							}

							if(reduceTrace)
							{
								// Die Spur dort etwas uninteressanter machen!
								level.setAITrace(position + bestDirV, bestTrace / 2);
							}
						}
						else
						{
							if(bestTrace > 850) interest = 160000;
							else if(bestTrace > 100) interest = 20000;
						}
					}
				}
				break;
			}

			if(position == targetPosition || interest < 10)
			{
				targetPosition = Vec2i(-1, -1);
				interest = 0;
			}

			moveCounter = random(4, 7);
		}

		if(height == 0.0)
		{
			if(!(random() % 25))
			{
				vy = random(40.0, 80.0);
				height = 0.5;
			}
		}
		else
		{
			height += 0.02 * vy;
			vy -= 0.02 * 400.0;

			if(height < 0.5)
			{
				height = 0.0;
				vy = 0.0;
			}
		}

		int pr = 700;
		if(interest >= 10000) pr = 350;
		if(!(random() % pr))
		{
			// Lachen abspielen
			Engine::inst().playSound("enemy2_laugh.ogg", false, 0.15, -100);
		}

		if(interest >= 40000)
		{
			if(!(random() % 200))
			{
				// Knurren abspielen
				Engine::inst().playSound("enemy2_growl.ogg", false, 0.15, -100);
			}
		}

		if(interest >= 10000)
		{
			// Feuer
			ParticleSystem* p_particleSystem = level.getParticleSystem();
			ParticleSystem* p_fireParticleSystem = level.getFireParticleSystem();
			ParticleSystem::Particle p;
			p.lifetime = random(40, 50);
			p.damping = 0.9f;
			p.gravity = -0.04f;
			p.positionOnTexture = Vec2b(32, 0);
			p.sizeOnTexture = Vec2b(16, 16);
			const double r = random(0.0, 6.283);
			const Vec2d vr(sin(r), cos(r));
			p.position = position * 16 + Vec2d(7.5, 7.5 - height) + 7.5 * vr;
			p.velocity = vr;
			p.color = Vec4d(random(0.5, 1.0), random(0.8, 1.0), random(0.0, 0.25), random(0.2, 0.4));
			const double dc = -1.5 / (p.lifetime + random(-25, 25));
			p.deltaColor = Vec4d(dc, dc, dc, -p.color.a / p.lifetime);
			p.rotation = random(0.0f, 10.0f);
			p.deltaRotation = random(-0.1f, 0.1f);
			p.size = random(0.5f, 0.9f);
			p.deltaSize = random(0.0075f, 0.015f);
			if(random() % 2) p_particleSystem->addParticle(p);
			else p_fireParticleSystem->addParticle(p);
		}
	}

	if(eatCounter) eatCounter--;
}

void Enemy::onCollect(Player* p_player)
{
	if(p_player->isTeleporting()) return;

	if(subType == 0)
	{
		if(!eatCounter)
		{
			ParticleSystem* p_particleSystem = level.getParticleSystem();
			ParticleSystem::Particle p;

			// die Metzelei hinter einer Staubwolke verstecken
			for(int i = 0; i < 150; i++)
			{
				p.lifetime = 100;
				p.damping = 0.99f;
				p.gravity = 0.005f;
				p.positionOnTexture = Vec2b(0, 0);
				p.sizeOnTexture = Vec2b(16, 16);
				p.position = position * 16 + Vec2i(random(4, 12), random(4, 12));
				double a = random(0.0, 1000.0);
				p.velocity = Vec2d(sin(a), cos(a)) * random(0.05, 1.0);
				double c = random(0.6, 1.0);
				p.color = p_player->getDebrisColor();
				p.color.a *= random(0.5f, 1.2f);
				p.deltaColor = Vec4d(0.0, 0.0, 0.0, -p.color.a / p.lifetime);
				p.rotation = random(0.0f, 10.0f);
				p.deltaRotation = random(-0.1f, 0.1f);
				p.size = random(0.3f, 0.5f);
				p.deltaSize = random(0.01f, 0.05f);
				p_particleSystem->addParticle(p);
			}

			Engine::inst().playSound("enemy1_eat.ogg", false, 0.1);
			p_player->disappear(1.5);
			p_player->censored = true;
			moveCounter = 130;
			eatCounter = 130;
			burpCounter = 100;

			slideDir = -1;
		}
	}
	else if(subType == 1)
	{
		if(!eatCounter)
		{
			ParticleSystem* p_particleSystem = level.getParticleSystem();
			ParticleSystem::Particle p;

			// die Metzelei hinter einer Staubwolke verstecken
			for(int i = 0; i < 150; i++)
			{
				p.lifetime = 100;
				p.damping = 0.99f;
				p.gravity = 0.005f;
				p.positionOnTexture = Vec2b(0, 0);
				p.sizeOnTexture = Vec2b(16, 16);
				p.position = position * 16 + Vec2i(random(4, 12), random(4, 12));
				double a = random(0.0, 1000.0);
				p.velocity = Vec2d(sin(a), cos(a)) * random(0.05, 1.0);
				double c = random(0.6, 1.0);
				p.color = p_player->getDebrisColor();
				p.color.a *= random(0.5f, 1.2f);
				p.deltaColor = Vec4d(0.0, 0.0, 0.0, -p.color.a / p.lifetime);
				p.rotation = random(0.0f, 10.0f);
				p.deltaRotation = random(-0.1f, 0.1f);
				p.size = random(0.3f, 0.5f);
				p.deltaSize = random(0.01f, 0.05f);
				p_particleSystem->addParticle(p);
			}

			Engine::inst().playSound("enemy2_eat.ogg", false, 0.1);
			p_player->disappear(1.5);
			p_player->censored = true;
			moveCounter = 130;
			eatCounter = 130;
			burpCounter = 100;

			slideDir = -1;
		}
	}
}

bool Enemy::changeInEditor(int mod)
{
	if(!mod)
	{
		dir++;
		dir %= 4;
		shownDir = dir;
	}
	else
	{
		subType++;
		subType %= 2;
	}

	return true;
}

void Enemy::saveAttributes(TiXmlElement* p_target)
{
	p_target->SetAttribute("subType", subType);
	p_target->SetAttribute("dir", dir);
}

void Enemy::saveExtendedAttributes(TiXmlElement* p_target)
{
	Object::saveExtendedAttributes(p_target);

	p_target->SetAttribute("contamination", contamination);
}

void Enemy::loadExtendedAttributes(TiXmlElement* p_element)
{
	Object::loadExtendedAttributes(p_element);

	p_element->Attribute("contamination", &contamination);
}

std::string Enemy::getToolTip() const
{
	switch(subType % 2)
	{
	case 0: return "$TT_ENEMY1";
	case 1: return "$TT_ENEMY2";
	}

	return toolTip;
}

void Enemy::setInvisibility(int invisibility)
{
	this->invisibility = invisibility;
}

bool Enemy::tryToMove(const Vec2i& dir)
{
	if(invisibility) return false;

	Vec2i np = position + dir;

	// Ist da Feuer?
	const std::vector<Object*> objects = level.getObjectsAt(np);
	for(std::vector<Object*>::const_iterator i = objects.begin(); i != objects.end(); ++i) if((*i)->getType() == "Fire") return false;

	// Ist da ein Laserstrahl, Lava oder Giftgas?
	if(level.getAIFlags(np) & (1 | 2 | 4)) return false;

	// Ist da Abgrund?
	uint tileID = level.getTileAt(0, np);
	const TileSet::TileInfo& tileInfo = level.getTileSet()->getTileInfo(tileID);
	if(tileInfo.type == 3)
	{
		// Wenn da ein Aufzug ist, ist es OK.
		if(level.getElevatorAt(np)) return move(dir, 1);
		else return false;
	}

	return move(dir, 1);
}

bool Enemy::canSee(const Vec2i& what)
{
	std::vector<Vec2i> points = bresenham(position, what);
	for(uint i = 1; i < points.size() - 1; i++)
	{
		if(!level.isFreeAt(points[i])) return false;
	}

	return true;
}