#ifndef _DEBRISCOLORDB_H
#define _DEBRISCOLORDB_H

#include "singleton.h"
#include <map>

/*** Klasse fuer die Datenbank der Truemmerfarben ***/

class Texture;

// Woher die Truemmer eines Objekts oder einer Kachel ihre Farbe nehmen.
//
// Frueher war das eine einzige Farbe: der alpha-gewichtete Mittelwert ueber die
// 16x16 Zelle, mit einer Streuung von +-0.1 je Kanal obendrauf. Beides war
// falsch. Der Mittelwert ueber ein Bild aus schwarzem Umriss, weissem Koerper
// und einer kleinen bunten Flaeche ist Grau - der gruene und der rote Knopf
// zerplatzten in genau denselben Staub. Und die Streuung zog jeden Kanal
// einzeln, verschob also den Farbton und erfand Rosa- und Gruentoene, die in
// der Grafik gar nicht vorkommen.
//
// Stattdessen wird jetzt aus der Textur selbst gezogen: eine Stelle in der
// Zelle wuerfeln, das Pixel dort ansehen, und es mit seiner eigenen Deckkraft
// als Wahrscheinlichkeit annehmen oder verwerfen. Das trifft die wirkliche
// Farbverteilung ohne jede Naeherung, braucht nichts Vorberechnetes - und die
// Zahl der Partikel richtet sich von selbst danach, wie viel von der Zelle
// ueberhaupt bedeckt ist: ein kleines Ding wirft weniger Truemmer als ein
// grosses.
// Wie viele Wuerfe auf einen gewuenschten Partikel kommen. Eine Zelle ist im
// Mittel zu 63% bedeckt, ein Wurf trifft also meistens; der Aufschlag holt
// heraus, was die durchsichtigen Stellen kosten. Er fuellt duenne Objekte
// bewusst nicht auf - die sollen ja weniger werfen. Das ist der einzige
// Stellknopf, wenn die Wolken zu duenn oder zu dicht wirken.
const int DEBRIS_TRIES_PER_PARTICLE = 2;

struct DebrisSource
{
	// 0 = keine Textur hinterlegt (die vier Objekte mit fest eingetragener
	// Farbe), oder die Pixel sind nicht mehr im Speicher. Dann gilt average.
	Texture* p_texture;
	Vec2i positionOnTexture;

	// Der alpha-gewichtete Mittelwert wie bisher. Rueckfall und Traeger der
	// Deckkraft, mit der die Emitter rechnen.
	Vec4d average;

	DebrisSource();

	void setTexture(Texture* p_texture, const Vec2i& positionOnTexture);
	void setColor(const Vec4d& color);

	// Eine Verwerfungsstichprobe. Liefert false, wenn an der gewuerfelten
	// Stelle nichts ist - dann entsteht kein Partikel. p_colorOut bekommt die
	// Farbe des Pixels mit der Deckkraft aus average, p_offsetOut die Stelle
	// innerhalb der Zelle, damit die Wolke die Form des Objekts behaelt.
	//
	// quarterTurns dreht diese Stelle so mit, wie das Bild gezeichnet wird.
	// Ohne das kaeme der Partikel aus der unrotierten Textur und saesse bei
	// einem gedrehten Objekt an der falschen Ecke. Kacheln werden nie gedreht.
	bool sample(Vec4d* p_colorOut, Vec2i* p_offsetOut, int quarterTurns = 0) const;
};

class DebrisColorDB : public Singleton<DebrisColorDB>
{
	friend class Singleton<DebrisColorDB>;

public:
	Vec4d getDebrisColor(Texture* p_texture, const Vec2i& positionOnTexture);

private:
	DebrisColorDB();
	~DebrisColorDB();

	typedef std::pair<std::string, Vec2i> dbKey;

	// Vec has an implicit 'operator const T*' so that it can be handed straight
	// to glColor4dv and friends. That conversion also makes 'a < b' on two Vec2i
	// decay to a POINTER comparison, so the default std::pair ordering compared
	// the addresses of the two vectors instead of their values - not a strict
	// weak ordering, and never equal for two separate objects. Every lookup below
	// therefore missed, the 16x16 average was recomputed on every call, and the
	// map grew by one entry per call for the lifetime of the process. Compare the
	// components explicitly.
	struct dbKeyLess
	{
		bool operator () (const dbKey& lhs, const dbKey& rhs) const
		{
			if(lhs.first != rhs.first) return lhs.first < rhs.first;
			if(lhs.second.x != rhs.second.x) return lhs.second.x < rhs.second.x;
			return lhs.second.y < rhs.second.y;
		}
	};

	typedef std::map<dbKey, Vec4d, dbKeyLess> dbMap;
	dbMap db;
};

#endif
