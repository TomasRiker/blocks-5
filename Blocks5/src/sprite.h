#ifndef _SPRITE_H
#define _SPRITE_H

/*** Klassen fuer das Aussehen eines Objekts oder einer Kachel ***/

class Texture;

// Deckkraft, mit der ein Truemmerpartikel startet.
const double DEBRIS_ALPHA = 0.25;

// Wuerfe je gewuenschtem Partikel; der Stellknopf fuer die Dichte der Wolke.
const int DEBRIS_TRIES_PER_PARTICLE = 2;

// Ein Teilbild. color ist die Eigenfaerbung des Objekts, nicht die des
// Renderdurchgangs - die multipliziert renderSprites() dazu.
struct Sprite
{
	static const int SIZE = 16;

	Vec2i positionOnTexture;
	Vec2i size;
	Vec2i offset;
	Vec4d color;
	bool mirrorX;
	double rotation;

	Sprite();
};

class Sprites
{
public:
	static const int MAX_SPRITES = 4;

	Sprites();

	void clear();
	Sprite& add(const Vec2i& positionOnTexture);
	Sprite& add(const Vec2i& positionOnTexture, const Vec4d& color);

	int getCount() const;
	const Sprite& operator [] (int index) const;

	void setTexture(Texture* p_texture);
	Texture* getTexture() const;

	// Kleinstes achsenparalleles Rechteck in Objektkoordinaten, das alle
	// Teilbilder mit Drehung und Versatz enthaelt.
	void getFootprint(Vec2i* p_minOut, Vec2i* p_maxOut) const;

	// Wuerfe fuer im Mittel numParticles Partikel; waechst mit der Flaeche.
	int getTryCount(int numParticles) const;

	// Verwerfungsstichprobe fuer die Farbe eines Truemmerpartikels. false
	// heisst: an der gewuerfelten Stelle ist nichts.
	bool sample(Vec4d* p_colorOut, Vec2i* p_offsetOut) const;

private:
	Sprite sprites[MAX_SPRITES];
	int numSprites;
	Texture* p_texture;
};

#endif
