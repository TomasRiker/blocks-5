#ifndef _SPRITE_H
#define _SPRITE_H

/*** Klassen fuer das Aussehen eines Objekts oder einer Kachel ***/

class Texture;

// Deckkraft, mit der ein Truemmerpartikel startet. Frueher steckte sie im
// alpha-gewichteten Mittelwert ueber die Zelle; den gibt es nicht mehr, also
// steht sie hier.
const double DEBRIS_ALPHA = 0.25;

// Wie viele Wuerfe auf einen gewuenschten Partikel kommen. Eine Zelle ist im
// Mittel zu 63% bedeckt, ein Wurf trifft also meistens; der Aufschlag holt
// heraus, was die durchsichtigen Stellen kosten. Er fuellt duenne Objekte
// bewusst nicht auf - die sollen ja weniger werfen. Das ist der einzige
// Stellknopf, wenn die Wolken zu duenn oder zu dicht wirken.
const int DEBRIS_TRIES_PER_PARTICLE = 2;

// Ein Teilbild: genau die Angaben, die Engine::renderSprite braucht.
//
// color ist die *Eigen*faerbung des Objekts - getStdColor(), der pulsende
// Ausgang, die halbdurchsichtige Flamme -, nicht die des Renderdurchgangs.
// Die kommt von aussen dazu und wird in renderSprites() dazumultipliziert.
// Der Unterschied ist wichtig: der Schattendurchgang zeichnet dasselbe Bild
// mit Schattenfarbe, und die haben die Truemmer nicht.
struct Sprite
{
	// Die uebliche Kantenlaenge eines Teilbilds auf sprites.png. Genauso gross
	// ist eine Kachel - was sprite.cpp mit einem static_assert festhaelt, weil
	// die Truemmer-Stichprobe auf beides dieselbe Rechnung anwendet. Nur
	// Damage ist groesser (46x46), deshalb steht die Groesse trotzdem im
	// Teilbild und nicht nur hier.
	static const int SIZE = 16;

	Vec2i positionOnTexture;
	Vec2i size;
	Vec2i offset;
	Vec4d color;
	bool mirrorX;

	// In Grad, genau wie der Parameter von renderSprite. Weiche Drehungen
	// (shownDir) kommen hier ungerundet an; die Truemmer-Stichprobe rechnet
	// mit dem Winkel selbst und nicht mit Vierteldrehungen.
	double rotation;

	Sprite();
};

// Die Teilbilder, aus denen ein Objekt oder eine Kachel besteht, in
// Zeichenreihenfolge: das zuletzt hinzugefuegte liegt vorn.
//
// Woher die Truemmer ihre Farbe nehmen: aus der Textur selbst. Eine Stelle
// im Umriss wuerfeln, nachsehen, welches Teilbild dort vorne liegt, und das
// Pixel mit seiner eigenen Deckkraft als Wahrscheinlichkeit annehmen oder
// verwerfen. Das trifft die wirkliche Farbverteilung ohne jede Naeherung,
// braucht nichts Vorberechnetes - und die Zahl der Partikel richtet sich von
// selbst danach, wie viel ueberhaupt bedeckt ist: ein kleines Ding wirft
// weniger Truemmer als ein grosses.
class Sprites
{
public:
	// Die meisten Objekte haben ein Teilbild, einige zwei (Kanone, Auge,
	// Spieler mit Gasmaske), E_HexDigit drei. Vier ist Luft nach oben.
	static const int MAX_SPRITES = 4;

	Sprites();

	void clear();

	// Fuegt ein Teilbild von Sprite::SIZE Kantenlaenge hinzu und liefert es
	// zurueck, damit der Aufrufer Drehung, Faerbung oder Versatz noch setzen
	// kann. Ist kein Platz mehr, wird das letzte ueberschrieben - das waere
	// ein Programmierfehler, und MAX_SPRITES gehoerte dann hochgesetzt.
	Sprite& add(const Vec2i& positionOnTexture);
	Sprite& add(const Vec2i& positionOnTexture, const Vec4d& color);

	int getCount() const;
	const Sprite& operator [] (int index) const;

	// Die Textur, aus der die Stichprobe zieht. Beim Zeichnen wird sie nicht
	// gebraucht - Level::renderObjects bindet sprites.png einmal um die ganze
	// Schleife herum.
	void setTexture(Texture* p_texture);
	Texture* getTexture() const;

	// Das kleinste achsenparallele Rechteck in Objektkoordinaten, das alle
	// Teilbilder enthaelt - mit Drehung und Versatz. Fuer ein einzelnes
	// ungedrehtes 16x16-Bild ist das genau (0,0)-(16,16).
	void getFootprint(Vec2i* p_minOut, Vec2i* p_maxOut) const;

	// Wie viele Wuerfe es braucht, um im Mittel numParticles Partikel zu
	// bekommen. Die Trefferquote eines Wurfs sinkt mit der Flaeche des
	// Rechtecks, in das geworfen wird - ein gedrehtes Bild belegt bis zu
	// sqrt(2) mal so viel Platz -, also muss die Zahl der Wuerfe damit
	// steigen. Sonst wuerfe dasselbe Objekt gedreht halb so viele Truemmer.
	int getTryCount(int numParticles) const;

	// Eine Verwerfungsstichprobe. Liefert false, wenn an der gewuerfelten
	// Stelle nichts ist - dann entsteht kein Partikel. p_colorOut bekommt die
	// Farbe des Pixels mal der Eigenfaerbung seines Teilbilds, p_offsetOut die
	// Stelle in Objektkoordinaten, damit die Wolke die Form des Objekts
	// behaelt. Die Stelle darf ausserhalb von (0,0)-(16,16) liegen: bei einem
	// gedrehten Bild gehoert sie dorthin, wo das Pixel wirklich zu sehen ist.
	bool sample(Vec4d* p_colorOut, Vec2i* p_offsetOut) const;

private:
	Sprite sprites[MAX_SPRITES];
	int numSprites;
	Texture* p_texture;
};

#endif
