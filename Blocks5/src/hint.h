#ifndef _HINT_H
#define _HINT_H

#include "object.h"

/*** Klasse fuer einen Hinweiszettel ***/

class Font;
class Texture;
class Player;

class Hint : public Object
{
public:
	Hint(Level& level, const Vec2i& position, const std::string& text);
	~Hint();

	void onRender(int layer, const Vec4d& color);
	void updateSprites();
	void onUpdate();
	void onRemove();
	void onCollect(Player* p_player);
	void saveAttributes(TiXmlElement* p_target);

	const std::string& getText() const;
	void setText(const std::string& text);

private:
	// Wohin der Zettel aufklappt. Muss feststehen, bevor das erste Bild davon
	// zu sehen ist - siehe onUpdate().
	void updateTargetPosition();

	// Zettel und Text zusammen in eine Textur zeichnen. Danach ist die Schrift
	// Teil des Papiers: sie fliegt mit, dreht mit und rollt sich mit auf,
	// statt am Ende darauf zu erscheinen.
	void bakeNote();

	// Das Papier als Streifen aus Vierecken: flach in der Mitte, oben und
	// unten eingerollt. unroll laeuft von 0 (ganz eingerollt) bis 1 (flach).
	void renderNote(const Vec4d& color, double unroll) const;
	void renderNoteMesh(const Vec4d& color, double unroll) const;

	// Der Weg ohne Bildpuffer: Zettel und Text nacheinander, ohne Rollen -
	// statt dessen schrumpft die Hoehe auf den flach liegenden Teil.
	void renderNoteFlat(const Vec4d& color, double unroll) const;

	std::string text;
	double alpha;
	double shownAlpha;
	Font* p_font;
	Texture* p_sprite;
	Vec2i targetPosition;

	// Die gebackene Textur samt dem Text, fuer den sie gilt. Sie ist von der
	// Engine geliehen und geht zurueck, sobald der Zettel nicht mehr zu sehen
	// ist - sonst hielte jeder je betretene Zettel eine ganze Bildschirmtextur
	// fest.
	uint noteTexture;
	std::string bakedText;

	void releaseNoteTexture();

	// Das Aufrollen: nach Logiktakten und nicht nach shownAlpha, das sich
	// seinem Ziel nur naehert und nie ankommt.
	double unroll;
	int activeTicks;
};

#endif
