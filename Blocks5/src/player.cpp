#include "pch.h"
#include "player.h"
#include "tileset.h"
#include "presets.h"
#include "engine.h"
#include "font.h"
#include "gui.h"
#include "texture.h"
#include "sound.h"
#include "soundinstance.h"
#include "particlesystem.h"
#include "debriscolordb.h"

uint Player::numInstances = 0;
std::list<Player*> Player::instances;
SoundInstance* Player::p_toxicSoundInst = 0;
SoundInstance* Player::p_maskSoundInst = 0;

Player::Player(Level& level,
			   const Vec2i& position,
			   uint character,
			   bool active) : Object(level, 0)
{
	warpTo(position);
	flags = OF_MASSIVE | OF_FIXED | OF_DESTROYABLE | OF_TRIGGER_PANELS | OF_TRANSPORTABLE | OF_BURSTABLE;
	destroyTime = 1;
	burstSound = "player_burst.ogg";

	interpolation = 0.3;
	this->character = character;
	this->active = false;
	if(active) activate();
	touch = 0;
	push = 0;
	walk = level.isInMenu() ? 0 : 40;
	plantBomb = level.isInMenu() ? 0 : 40;
	censored = false;
	contamination = 0;

	positionOnTexture = Vec2i(character * 64, 224);

	for(int i = 0; i < 8; i++) inventory[i] = 0;

	if(!level.isInEditor() && !level.isInCat())
	{
		numInstances++;
		instances.push_back(this);

		if(numInstances == 1)
		{
			// Das ist die erste Instanz. Soundinstanz erzeugen und pausieren.
			Sound* p_sound = Manager<Sound>::inst().request("toxic.ogg");
			p_toxicSoundInst = p_sound->createInstance();
			p_sound->release();

			p_toxicSoundInst->setVolume(0.0);
			p_toxicSoundInst->setPitch(0.1);
			p_toxicSoundInst->play(true);
			p_toxicSoundInst->pause();

			p_sound = Manager<Sound>::inst().request("mask.ogg");
			p_maskSoundInst = p_sound->createInstance();
			p_sound->release();

			p_maskSoundInst->setVolume(0.0);
			p_maskSoundInst->setPitch(0.1);
			p_maskSoundInst->play(true);
			p_maskSoundInst->pause();
		}
	}

	if(level.isInCat()) this->active = true;
}

Player::~Player()
{
}

void Player::onRemove()
{
	if(level.getActivePlayer() == this && p_toxicSoundInst && p_maskSoundInst)
	{
		p_toxicSoundInst->slideVolume(-1.0, 0.1);
		p_toxicSoundInst->slidePitch(1.0, 0.1);
		p_maskSoundInst->slideVolume(-1.0, 0.2);
		p_maskSoundInst->slidePitch(1.0, 0.2);
	}

	deactivate();

	if(!level.isInEditor() && !level.isInCat())
	{
		numInstances--;
		instances.remove(this);

		if(!numInstances)
		{
			// Das war die letzte Instanz. Sound stoppen.
			p_toxicSoundInst->stop();
			p_toxicSoundInst = 0;
			p_maskSoundInst->stop();
			p_maskSoundInst = 0;
		}
	}
}

void Player::onRender(int layer,
					  const Vec4d& color)
{
	if(layer == 1)
	{
		// Spieler rendern
		Vec2i positionOnTexture(character * 64 + (active ? 0 : 32), 224);
		Engine::inst().renderSprite(Vec2i(0, 0), positionOnTexture, Vec2i(16, 16), color);

		if(inventory[2])
		{
			// Gasmaske rendern
			positionOnTexture = Vec2i(224, 416);
			Engine::inst().renderSprite(Vec2i(0, character == 0 ? 3 : 4), positionOnTexture, Vec2i(16, 16), color);
		}
	}
	else if(layer == 16)
	{
		if(censored)
		{
			// Zensierbalken rendern
			glDisable(GL_TEXTURE_2D);
			glPushMatrix();
			glTranslated(8.0, 8.0, 0.0);
			glRotated(10.0, 0.0, 0.0, 1.0);
			glBegin(GL_QUADS);
			glColor3d(0.15, 0.15, 0.15);
			glVertex2i(-35, -13);
			glVertex2i(35, -13);
			glVertex2i(35, 13);
			glVertex2i(-35, 13);
			glEnd();
			glBegin(GL_LINE_LOOP);
			glVertex2i(-35, -13);
			glVertex2i(35, -13);
			glVertex2i(35, 13);
			glVertex2i(-35, 13);
			glEnd();
			glPopMatrix();
			glEnable(GL_TEXTURE_2D);

			Font* p_font = GUI::inst().getFont();
			std::string text = localizeString("$G_CENSORED");
			Vec2i dim;
			p_font->measureText(text, &dim, 0);
			p_font->renderText(text, Vec2i(8, 7) + dim / -2, Vec4d(0.85, 0.15, 0.15, 1.0));
			level.getSprites()->bind();
		}
	}
	else if(layer == 18)
	{
		level.renderShine(active ? 1.0 : 0.5, (active ? 1.0 : 0.5) + random(-0.05, 0.05));
	}
}

void Player::onUpdate()
{
	Engine& engine = Engine::inst();

	if(touch) touch--;
	if(push) push--;
	if(walk) walk--;
	if(plantBomb) plantBomb--;

	// Spur für die Gegner legen
	level.setAITrace(position, 1000);

	if(!inventory[2])
	{
		if(level.getAIFlags(position) & 2)
		{
			// Vergiftung
			contamination++;

			if(active && contamination == 50) updateToxicSound();
		}
	}

	if(contamination >= 500)
	{
		// sterben
		burst();
	}

	if(active && !level.isInPreview())
	{
		if(!walk)
		{
			Vec2i dir(0, 0);
			bool shift, ctrl;

			if(level.isInMenu())
			{
				shift = engine.isKeyDown(SDLK_LSHIFT) || engine.isKeyDown(SDLK_RSHIFT);
				ctrl = engine.isKeyDown(SDLK_LCTRL) || engine.isKeyDown(SDLK_RCTRL);

				if(engine.wasKeyPressed(SDLK_LEFT)) dir.x = -1;
				else if(engine.wasKeyPressed(SDLK_RIGHT)) dir.x = 1;
				else if(engine.wasKeyPressed(SDLK_UP)) dir.y = -1;
				else if(engine.wasKeyPressed(SDLK_DOWN)) dir.y = 1;

				if(engine.wasKeyReleased(SDLK_LEFT) ||
				   engine.wasKeyReleased(SDLK_RIGHT) ||
				   engine.wasKeyReleased(SDLK_UP) ||
				   engine.wasKeyReleased(SDLK_DOWN))
				{
					touch = 0;
				}
			}
			else
			{
				shift = engine.isActionDown("$A_PLANT_BOMB");
				ctrl = engine.isActionDown("$A_PUT_DOWN_BOMB");

				if(engine.wasActionPressed("$A_LEFT")) dir.x = -1;
				else if(engine.wasActionPressed("$A_RIGHT")) dir.x = 1;
				else if(engine.wasActionPressed("$A_UP")) dir.y = -1;
				else if(engine.wasActionPressed("$A_DOWN")) dir.y = 1;

				if(engine.wasActionReleased("$A_LEFT") ||
				   engine.wasActionReleased("$A_RIGHT") ||
				   engine.wasActionReleased("$A_UP") ||
				   engine.wasActionReleased("$A_DOWN"))
				{
					touch = 0;
				}
			}

			if(shift)
			{
				if(inventory[0] && !plantBomb && (dir.x || dir.y))
				{
					// Bombe legen
					Object* p_bomb = level.getPresets()->instancePreset("Bomb", position, 0);
					if(p_bomb->move(dir))
					{
						Engine::inst().playSound("bomb_plant.ogg", false, 0.15, 100);
						p_bomb->setFlags(p_bomb->getFlags() & ~OF_COLLECTABLE);
						inventory[0]--;
					}
					else
					{
						p_bomb->disappear(0.0);
					}

					plantBomb = 20;
				}
			}
			else if(ctrl)
			{
				if(inventory[0] && !plantBomb && (dir.x || dir.y))
				{
					// Bombe unangezündet legen
					Object* p_bomb = level.getPresets()->instancePreset("Bomb", position, 0);
					if(p_bomb->move(dir))
					{
						Engine::inst().playSound("bomb_plant.ogg", false, 0.15, 100);
						inventory[0]--;
						p_bomb->noCollect = 0.25;
					}
					else
					{
						p_bomb->disappear(0.0);
					}

					plantBomb = 20;
				}
			}
			else
			{
				if(slideDir == -1 && (dir.x || dir.y)) move(dir);
			}
		}
	}
	else if(!level.isInPreview())
	{
		if(!(level.counter % 20))
		{
			// Schnarchpartikel erzeugen
			ParticleSystem* p_particleSystem = level.getParticleSystem();
			ParticleSystem::Particle p;
			p.lifetime = random(50, 100);
			p.damping = 0.99f;
			p.gravity = 0.0f;
			p.positionOnTexture = Vec2b(64, 32);
			p.sizeOnTexture = Vec2b(16, 16);
			p.position = position * 16 + Vec2i(8 + random(-2, 2), random(-6, 0));
			p.velocity = Vec2d(random(-0.25, 0.25), -0.6);
			p.color = Vec4d(random(0.8, 1.0), random(0.8, 1.0), random(0.8, 1.0), 0.8);
			p.deltaColor = Vec4d(0.0, 0.0, 0.0, -p.color.a / p.lifetime);
			p.rotation = random(-0.5f, 0.5f);
			p.deltaRotation = random(-0.01f, 0.01f);
			p.size = random(0.6f, 0.8f);
			p.deltaSize = random(0.005f, 0.02f);
			p_particleSystem->addParticle(p);
		}
	}
}

bool Player::move(const Vec2i& dir,
				  bool deadlyWeight)
{
	if(slideDir != -1 && !slideMove) return false;
	if(dir.isZero()) return true;
	if(moved) return false;
	if(!level.isValidPosition(position + dir)) return false;

	interpolation = 0.3;

	int tileType = 0;
	if(level.isFreeAt(position + dir, &tileType))
	{
		position += dir;
		moved = true;
		lastMoveDir = dir;
		level.hashObject(this);
		handleSliding();
		return true;
	}
	else if(tileType != 1 && tileType != 2)
	{
		Object* p_obj = level.getFrontObjectAt(position + dir);
		if(p_obj)
		{
			// Ist es eine andere Spielfigur?
			if(p_obj->getType() == "Player")
			{
				if(!push || deadlyWeight)
				{
					if(static_cast<Player*>(p_obj)->move(dir, true))
					{
						position += dir;
						moved = true;
						lastMoveDir = dir;
						level.hashObject(this);
						handleSliding();

						if(!deadlyWeight)
						{
							push = 5;
							interpolation = 0.25;
						}

						return true;
					}
				}
			}

			// Kann man das Objekt einsammeln?
			if(p_obj->getFlags() & OF_COLLECTABLE)
			{
				// Objekt einsammeln
				position += dir;
				moved = true;
				lastMoveDir = dir;
				level.hashObject(this);
				handleSliding();
				return true;
			}

			// Ist es ein verschiebbares Objekt?
			if(!push && !(p_obj->getFlags() & OF_FIXED) && !p_obj->isPushedWithDeadlyWeight())
			{
				// Objekte, die von der Gravitation oder von anderen Objekten von oben beeinflusst werden,
				// können nur nach links und rechts geschoben werden.
				bool pushed = p_obj->isPushedFromAbove();
				if((pushed && !dir.y) || !pushed)
				{
					// versuchen, das Objekt zu verschieben
					if(p_obj->move(dir, 10))
					{
						// Es hat geklappt.

						Engine::inst().playSound("push.ogg", false, 0.2);

						lastMoveDir = dir;
						handleSliding();

						if(slideDir < 0)
						{
							position += dir;
							moved = true;
							lastMoveDir = dir;
							level.hashObject(this);
							push = 5;
							interpolation = 0.25;
							return true;
						}
						else slideDir = -2;
					}
				}
			}

			// Ist es ein pfeilartiges Objekt?
			if(p_obj->getFlags() & OF_ARROWTYPE)
			{
				if(p_obj->allowMovement(dir))
				{
					// Bewegung ist in Ordnung.
					position += dir;
					moved = true;
					lastMoveDir = dir;
					level.hashObject(this);
					handleSliding();
					return true;
				}
			}

			if(!touch)
			{
				p_obj->onTouchedByPlayer(this);
				touch = 20;
			}
		}
	}

	slideDir = -1;
	return false;
}

bool Player::changeInEditor(int mod)
{
	if(!mod) activate();
	else
	{
		character++;
		character %= 3;

		positionOnTexture = Vec2i(character * 64, 224);
		debrisColor = DebrisColorDB::inst().getDebrisColor(level.getSprites(), positionOnTexture);
	}

	return true;
}

void Player::saveAttributes(TiXmlElement* p_target)
{
	p_target->SetAttribute("character", character);
	p_target->SetAttribute("active", active ? 1 : 0);
}

void Player::saveExtendedAttributes(TiXmlElement* p_target)
{
	Object::saveExtendedAttributes(p_target);

	p_target->SetAttribute("contamination", contamination);
	for(int i = 0; i < 8; i++)
	{
		char attrName[256] = "";
		sprintf(attrName, "inventory%d", i);
		p_target->SetAttribute(attrName, inventory[i]);
	}

	p_target->SetAttribute("walk", walk);
	p_target->SetAttribute("touch", touch);
	p_target->SetAttribute("push", push);
	p_target->SetAttribute("plantBomb", plantBomb);
}

void Player::loadExtendedAttributes(TiXmlElement* p_element)
{
	Object::loadExtendedAttributes(p_element);

	p_element->Attribute("contamination", &contamination);
	for(int i = 0; i < 8; i++)
	{
		char attrName[256] = "";
		sprintf(attrName, "inventory%d", i);
		int inv;
		p_element->Attribute(attrName, &inv);
		inventory[i] = inv;
	}

	p_element->Attribute("walk", &walk);
	p_element->Attribute("touch", &touch);
	p_element->Attribute("push", &push);
	p_element->Attribute("plantBomb", &plantBomb);

	updateToxicSound();
	updateMaskSound();
}

std::string Player::getToolTip() const
{
	switch(character % 3)
	{
	case 0: return "$TT_PLAYER1";
	case 1: return "$TT_PLAYER2";
	case 2: return "$TT_PLAYER3";
	}

	return toolTip;
}

bool Player::isActive() const
{
	return active;
}

void Player::activate()
{
	if(active) return;

	// alle Spieler deaktivieren
	const std::vector<Object*>& objects = level.getObjects();
	for(std::vector<Object*>::const_iterator i = objects.begin(); i != objects.end(); ++i)
	{
		if((*i)->getType() == "Player")
		{
			Player* p_player = static_cast<Player*>(*i);
			p_player->deactivate();
		}
	}

	// mich selbst aktivieren
	active = true;
	level.p_activePlayer = this;

	if(!level.isInEditor() && level.counter)
	{
		// Sound
		char soundName[256];
		sprintf(soundName, "character%d.ogg", character + 1);
		Engine::inst().playSound(soundName, false, 0.15, 100);

		// Sterne
		ParticleSystem* p_particleSystem = level.getParticleSystem();
		ParticleSystem::Particle p;
		for(int i = 0; i < 50; i++)
		{
			p.lifetime = random(20, 50);
			p.damping = 0.85f;
			p.gravity = 0.0f;
			p.positionOnTexture = Vec2b(0, 32);
			p.sizeOnTexture = Vec2b(16, 16);
			p.position = position * 16 + Vec2i(8, 8);
			const double r = random(0.0, 6.283);
			p.velocity = random(2.0, 3.0) * Vec2d(sin(r), cos(r));
			p.color = Vec4d(random(0.75, 1.0), random(0.75, 1.0), random(0.75, 1.0), random(0.6, 0.9));
			p.deltaColor = Vec4d(0.0, 0.0, 0.0, -p.color.a / p.lifetime);
			p.rotation = random(0.0f, 10.0f);
			p.deltaRotation = random(-0.1f, 0.1f);
			p.size = random(0.25f, 0.5f);
			p.deltaSize = random(0.0f, 0.01f);
			p_particleSystem->addParticle(p);
		}

		updateToxicSound();
		updateMaskSound();
	}
}

void Player::deactivate()
{
	if(!active) return;

	active = false;
	level.p_activePlayer = 0;
}

uint Player::getInventory(uint index)
{
	if(index >= 8) return 0;
	else return inventory[index];
}

bool Player::addInventory(uint index,
						  int add)
{
	if(index >= 8) return false;

	if(index == 1)
	{
		// Diamant!
		level.setNumDiamondsCollected(level.getNumDiamondsCollected() + 1);
		return true;
	}
	else if(index == 2)
	{
		if(inventory[index])
		{
			// Man kann nur eine einzige Maske tragen.
			return false;
		}
	}
	else if(index == 3)
	{
		// Spritze
		contamination -= 600;
		updateToxicSound();
		return true;
	}

	inventory[index] += add;

	if(index == 2) updateMaskSound();

	return true;
}

int Player::getContamination() const
{
	return contamination;
}

uint Player::getNumInstances()
{
	return numInstances;
}

const std::list<Player*>& Player::getInstances()
{
	return instances;
}

void Player::updateToxicSound()
{
	if(!p_toxicSoundInst) return;
	if(level.getActivePlayer() != this) return;

	if(contamination >= 50)
	{
		p_toxicSoundInst->resume();
		p_toxicSoundInst->slideVolume(1.0, 0.05);
		p_toxicSoundInst->slidePitch(1.0, 0.05);
	}
	else
	{
		p_toxicSoundInst->slideVolume(-1.0, 0.1);
		p_toxicSoundInst->slidePitch(1.0, 0.1);
	}
}

void Player::updateMaskSound()
{
	if(!p_maskSoundInst) return;
	if(level.getActivePlayer() != this) return;

	if(inventory[2])
	{
		p_maskSoundInst->resume();
		p_maskSoundInst->slideVolume(1.0, 0.2);
		p_maskSoundInst->slidePitch(1.0, 0.2);
	}
	else
	{
		p_maskSoundInst->slideVolume(-1.0, 0.2);
		p_maskSoundInst->slidePitch(1.0, 0.2);
	}
}