#include "pch.h"
#include "hint.h"
#include "player.h"
#include "engine.h"
#include "font.h"
#include "texture.h"

Hint::Hint(Level& level,
		   const Vec2i& position,
		   const std::string& text) : Object(level, 2)
{
	warpTo(position);
	flags = OF_FIXED | OF_TRANSPORTABLE | OF_COLLECTABLE;
	this->text = text;
	alpha = shownAlpha = 0.0;
	// Vec2i hat keinen initialisierenden Standardkonstruktor.
	targetPosition = Vec2i(320, 200);

	p_sprite = level.getHint();
	p_font = level.getHintFont();

	Font::Options options = p_font->getOptions();
	options.charSpacing = -1;
	options.lineSpacing = 0.95;
	options.shadows = 1;
	p_font->setOptions(options);
}

Hint::~Hint()
{
}

void Hint::updateSprites()
{
	// Zettelobjekt
	sprites.add(Vec2i(96, 288));
}

void Hint::onRender(int layer,
					const Vec4d& color)
{
	if(layer == 1) Engine::inst().renderSprites(sprites, color);
	else if(layer == 42 || layer == 43)
	{
		double a = shownAlpha;
		double r = (0.85 - shownAlpha) * 45.0;
		double i = shownAlpha / 0.85;
		double s = i * 0.9;

		// Layer 43 ist die Vorschau im Leveleditor: fertig aufgeklappt, mittig.
		// Das ist eine Anzeigesache und darf targetPosition nicht veraendern -
		// sonst zeigt der Zettel im Spiel hinterher woandershin.
		Vec2i target = targetPosition;
		if(layer == 43) a = 0.85, r = 0.0, i = 1.0, s = 0.9, target = Vec2i(320, 200);

		if(a > 1.0 / 255.0)
		{
			glPushMatrix();
			Vec2i p = -getShownPositionInPixels();
			glTranslated(p.x, p.y, 0.0);

			// Zettel anzeigen
			Vec4d realColor(color.r, color.g, color.b, color.a * a);
			p_sprite->bind();

			glPushMatrix();
			Vec2d sp = (1.0 - i) * static_cast<Vec2d>(getShownPositionInPixels()) + i * static_cast<Vec2d>(target);
			glTranslated(sp.x, sp.y, 0.0);
			glScaled(s, s, 1.0);
			glRotated(r, 0.0, 0.0, 1.0);
			Engine::inst().renderSprite(Vec2i(-145, -195), Vec2i(0, 0), Vec2i(300, 400), Vec4d(0.0, 0.0, 0.0, realColor.a * 0.3));
			Engine::inst().renderSprite(Vec2i(-150, -200), Vec2i(0, 0), Vec2i(300, 400), realColor);
			glPopMatrix();

			if(a > 0.84)
			{
				Vec4d textColor = color;
				textColor.a *= 65.0 * (a - 0.84);
				std::string text = localizeString(this->text);
				p_font->renderText(p_font->adjustText(text, 230), Vec2i(target.x - 115, target.y - 155), textColor);
			}

			glPopMatrix();
		}
	}
}

void Hint::onUpdate()
{
	// Spieler da?
	Object* p_obj = level.getFrontObjectAt(position);
	const bool playerIsHere = (p_obj == level.getActivePlayer());

	// Das Ziel muss schon feststehen, wenn der Zettel anfaengt aufzuklappen.
	// Frueher stand es nur in onCollect(), und das laeuft erst, wenn der
	// Spieler bis auf sechs Pixel mittig auf dem Feld steht (object.cpp) -
	// waehrend hier die blosse Feldposition zaehlt. Dazwischen liegen ein paar
	// Ticks, in denen der Zettel schon sichtbar ist und noch auf das Ziel vom
	// letzten Mal zulaeuft. Genau das war der Sprung beim wiederholten Betreten.
	if(playerIsHere) updateTargetPosition();

	alpha = playerIsHere ? 0.85 : 0.0;
	shownAlpha = 0.15 * alpha + 0.85 * shownAlpha;
	if(shownAlpha <= 1.0 / 255.0) shownAlpha = 0.0;
}

void Hint::updateTargetPosition()
{
	Player* p_player = level.getActivePlayer();
	if(!p_player) return;

	// Der Zettel soll den Spieler nicht verdecken.
	targetPosition = Vec2i(320, 200);
	const Vec2i pp = p_player->getPosition() * 16;
	if(pp.x >= 140 && pp.x <= 490)
	{
		if(pp.x < 320) targetPosition.x = 470;
		else targetPosition.x = 170;
	}
}

void Hint::onCollect(Player* p_player)
{
	updateTargetPosition();
}

void Hint::saveAttributes(TiXmlElement* p_target)
{
	TiXmlElement* p_text = new TiXmlElement("Text");
	TiXmlText* p_data = new TiXmlText(text);
	p_data->SetCDATA(true);
	p_text->LinkEndChild(p_data);
	p_target->LinkEndChild(p_text);
}

const std::string& Hint::getText() const
{
	return text;
}

void Hint::setText(const std::string& text)
{
	this->text = text;
}