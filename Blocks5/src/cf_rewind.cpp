#include "pch.h"
#include "cf_rewind.h"
#include "engine.h"
#include "font.h"
#include "gui.h"

namespace
{
	// Wie hoch ein Streifen ist. Ein Videokopf liest eine Spur, und eine Spur
	// ist ein Halbbild - hier sind es ein paar Zeilen, fein genug, dass die
	// Naht zwischen zwei Vorlagen nirgends als gerade Kante stehenbleibt, und
	// grob genug, dass 160 Streifen und nicht 480 zu zeichnen sind.
	const int STRIP_HEIGHT = 3;

	// Wie weit das Bild waehrend des Ruecklaufs durchlaeuft, in Bildhoehen.
	// Das Band rast, also rollt das Bild - der Bildfang haelt bei dem Tempo
	// nicht mehr mit.
	//
	// Eine ganze Zahl, und das ist keine Willkuer: am Ende steht der Versatz
	// dann auf einem Vielfachen der Bildhoehe, also wieder genau auf null. Bei
	// 6.5 saesse das Bild im letzten Augenblick um eine halbe Hoehe verrutscht
	// und sprungte beim Abblenden zurueck.
	//
	// Zehn und nicht sieben, weil die Ueberblendung von 1,1 auf 1,5 Sekunden
	// verlaengert wurde: die Zahl ist ein Weg und keine Geschwindigkeit, und
	// sieben Hoehen in anderthalb Sekunden waeren gemuetlich statt hektisch.
	const double ROLL_SCREENS = 10.0;

	// Ueber welchen Teil des Schlusses das Laufwerk bremst und der Bildfang
	// wieder einrastet. Ohne das hoerte der Bildsalat mit einem Schnitt auf.
	const double SETTLE = 0.18;

	// Seitlicher Versatz je Streifen: der Kopf liest die Spur schraeg an, also
	// faengt jede Zeile ein Stueck zu frueh oder zu spaet an.
	const double TRACKING_JITTER = 2.5;

	// Und dasselbe an der Naht, wo die beiden Vorlagen aufeinandertreffen -
	// dort ist die Spurlage am schlechtesten.
	const double SEAM_SHIFT = 16.0;

	// Wie ausgefranst die Naht ist, als Anteil der Bildhoehe. Waere sie scharf,
	// sae man eine Kante quer durchs Bild wandern und dahinter das fertige neue
	// Bild - genau das, was hier verborgen werden soll.
	const double SEAM_WIDTH = 0.30;

	// Rauschbalken: wo der Kopf zwischen zwei Spuren geraet, kommt gar kein
	// Bild, sondern Schnee. Wie viele es sind, haengt am Tempo des Bandes.
	const int NOISE_BARS = 5;
	const int NOISE_BAR_MIN = 6;
	const int NOISE_BAR_MAX = 22;

	// Der Schnee ueber dem ganzen Bild und der graue Schleier darueber. Der
	// Schleier ist die halbe Miete: VHS legt die Farbe als eigenen Traeger
	// unter das Bild, und der ueberlebt das Spulen nicht - ein gesuchtes Bild
	// ist fast grau.
	const double SNOW_ALPHA = 0.16;
	const double WASH_ALPHA = 0.22;

	// Kantenlaenge des Rauschbildes. Zweierpotenz, weil es gekachelt wird.
	const int NOISE_SIZE = 256;

	// Wo die Einblendung des Rekorders steht. Weit genug herein, dass die
	// Woelbung des CRT-Filters sie nicht an der Ecke abschneidet.
	const int OSD_X = 30;
	const int OSD_Y = 26;

	double wrap(double value, double range)
	{
		value = fmod(value, range);
		return (value < 0.0) ? value + range : value;
	}
}

CF_Rewind::CF_Rewind()
{
	p_font = GUI::inst().getFont();

	// Das Laufwerk. Der Ton gehoert dem Effekt und nicht der Stelle, die ihn
	// ausloest: es gibt nur einen Weg hierher, und so kann keiner den einen
	// ohne den anderen bekommen. Er ist etwas laenger als die Ueberblendung,
	// damit das Ausrollen nicht mit dem Bild zusammen abgeschnitten wird.
	Engine::inst().playSound("rewind.ogg", false, 0.0, 100);

	// Schnee, ein fuer alle Mal. Grau und nicht bunt: was der Kopf zwischen
	// zwei Spuren aufnimmt, ist Rauschen ohne Farbtraeger.
	unsigned char* p_pixels = new unsigned char[NOISE_SIZE * NOISE_SIZE * 3];
	for(int i = 0; i < NOISE_SIZE * NOISE_SIZE; i++)
	{
		const unsigned char v = static_cast<unsigned char>(randomInt() & 255);
		p_pixels[i * 3 + 0] = v;
		p_pixels[i * 3 + 1] = v;
		p_pixels[i * 3 + 2] = v;
	}

	glGenTextures(1, &noiseID);
	glBindTexture(GL_TEXTURE_2D, noiseID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, NOISE_SIZE, NOISE_SIZE, 0,
				 GL_RGB, GL_UNSIGNED_BYTE, p_pixels);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0);

	delete[] p_pixels;
}

CF_Rewind::~CF_Rewind()
{
	// Die Engine loescht die Ueberblendung in der Hauptschleife, der
	// GL-Kontext steht also noch.
	glDeleteTextures(1, &noiseID);
}

void CF_Rewind::drawStrip(int y,
						  int height,
						  int sourceY,
						  double shift) const
{
	// Die Texturkoordinaten stehen in Bildpunkten, siehe
	// Crossfade::setupTexCoords().
	glBegin(GL_QUADS);
	glTexCoord2d(shift, sourceY);
	glVertex2i(0, y);
	glTexCoord2d(shift + screenSize.x, sourceY);
	glVertex2i(screenSize.x, y);
	glTexCoord2d(shift + screenSize.x, sourceY + height);
	glVertex2i(screenSize.x, y + height);
	glTexCoord2d(shift, sourceY + height);
	glVertex2i(0, y + height);
	glEnd();
}

void CF_Rewind::drawSnow(int y,
						 int height,
						 double alpha) const
{
	const double u = random(0.0, 1.0);
	const double v = random(0.0, 1.0);
	const double du = static_cast<double>(screenSize.x) / NOISE_SIZE;
	const double dv = static_cast<double>(height) / NOISE_SIZE;

	glColor4d(1.0, 1.0, 1.0, alpha);
	glBegin(GL_QUADS);
	glTexCoord2d(u, v);
	glVertex2i(0, y);
	glTexCoord2d(u + du, v);
	glVertex2i(screenSize.x, y);
	glTexCoord2d(u + du, v + dv);
	glVertex2i(screenSize.x, y + height);
	glTexCoord2d(u, v + dv);
	glVertex2i(0, y + height);
	glEnd();
}

/* Warum ein Ruecklauf und nicht irgendein Effekt: beim Neustart springt das
   Spiel vom jetzigen Stand auf den Anfang, ohne irgendetwas dazwischen. Ein
   Videorekorder im Bildsuchlauf tut genau dasselbe und keiner stoert sich
   daran - weil das Band schneller laeuft, als der Kopf einer Spur folgen kann,
   und jeder Streifen des Bildes daher von einer anderen Stelle des Bandes
   stammt, also aus einem anderen Augenblick. Streifen aus zwei Bildern
   nebeneinander sind hier keine Notluege, sondern das, was ein Rekorder
   wirklich liefert.

   Was daraus folgt und den Sprung verdeckt:

   - Zwischen den Spuren liegt nichts, also kommt dort Schnee. Das sind die
     Rauschbalken, die durchs Bild wandern.
   - Der Bildfang haelt nicht mehr mit, also rollt das Bild.
   - Der Kopf trifft die Spur schraeg, also verrutscht jede Zeile ein Stueck
     seitlich - das Bild franst aus.
   - VHS traegt die Farbe getrennt und tieffrequent unter dem Bild; das
     ueberlebt den Suchlauf nicht. Deshalb der graue Schleier.

   Eines darf ausdruecklich NICHT zittern: die Einblendung "<< REW". Die kommt
   aus dem Zeichengenerator des Rekorders und wird hinter dem Bandweg
   zugemischt. Sie steht ruhig, waehrend alles andere reisst - und genau das
   macht aus dem Bildsalat ein Geraet. */
void CF_Rewind::render(double t,
					   uint oldImageID,
					   uint newImageID)
{
	Engine& engine = Engine::inst();
	setupTexCoords();

	glEnable(GL_TEXTURE_2D);
	glColor4d(1.0, 1.0, 1.0, 1.0);

	// Das Band faehrt an und bremst wieder ab.
	const double eased = t * t * (3.0 - 2.0 * t);
	const double roll = eased * ROLL_SCREENS * screenSize.y;

	// Und zum Schluss faengt sich das Bild: Spurlage, Schnee und Schleier gehen
	// zurueck, waehrend das Band ausrollt.
	const double settle = clamp((1.0 - t) / SETTLE, 0.0, 1.0);

	// Die Naht wandert von unten nach oben durchs Bild: das Band laeuft
	// rueckwaerts. Ueber SEAM_WIDTH hinweg entscheidet der Zufall, damit sie
	// keine Kante ist.
	const double seam = (1.0 + SEAM_WIDTH) * (1.0 - t) - 0.5 * SEAM_WIDTH;

	for(int y = 0; y < screenSize.y; y += STRIP_HEIGHT)
	{
		const int height = min(STRIP_HEIGHT, screenSize.y - y);
		const double where = static_cast<double>(y) / screenSize.y;

		// Ober- und unterhalb der Naht ist die Sache klar, dazwischen nicht.
		const double distance = (where - seam) / SEAM_WIDTH;
		const bool useNew = (distance + random(-0.5, 0.5) > 0.0);

		// Wie schlecht die Spurlage hier ist: an der Naht am schlechtesten.
		const double closeness = clamp(1.0 - fabs(distance), 0.0, 1.0);
		const double shift = settle * (random(-TRACKING_JITTER, TRACKING_JITTER)
									 + closeness * random(-SEAM_SHIFT, SEAM_SHIFT));

		// Die Zeile, die dieser Streifen zeigt. Von Hand umgebrochen und nicht
		// dem GL_REPEAT der Textur ueberlassen: das Bild fuellt nur 480 der 512
		// Zeilen, der Rest der Zweierpotenz ist nie beschrieben worden.
		const int sourceY = static_cast<int>(wrap(y + roll, screenSize.y));
		const int overlap = sourceY + height - screenSize.y;

		glBindTexture(GL_TEXTURE_2D, useNew ? newImageID : oldImageID);
		if(overlap <= 0) drawStrip(y, height, sourceY, shift);
		else
		{
			// Genau hier liegt beim Rekorder der Kopfumschaltpunkt: das Ende
			// des einen Halbbildes und der Anfang des naechsten, mit einem
			// gerissenen Streifen dazwischen. Also beide Haelften einzeln, mit
			// verschiedenem Versatz.
			drawStrip(y, height - overlap, sourceY, shift);
			drawStrip(y + height - overlap, overlap, 0,
					  shift + settle * random(-SEAM_SHIFT, SEAM_SHIFT));
		}
	}

	// --- Rauschen ---------------------------------------------------------
	// Eigene Texturmatrix: das Rauschbild wird in 0..1 abgetastet, nicht in
	// Bildpunkten des Schirms.
	glMatrixMode(GL_TEXTURE);
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glBindTexture(GL_TEXTURE_2D, noiseID);

	// Die Balken wandern nach unten und sind deckend: dort liegt kein Bild.
	for(int i = 0; i < NOISE_BARS; i++)
	{
		const double speed = 0.6 + 0.5 * i;
		const int height = random(NOISE_BAR_MIN, NOISE_BAR_MAX);
		const int y = static_cast<int>(wrap((static_cast<double>(i) / NOISE_BARS + eased * speed)
											* screenSize.y, screenSize.y));
		drawSnow(y, min(height, screenSize.y - y), settle);
	}

	// Und der Schnee ueber allem, addiert.
	engine.setBlendFunc(GL_SRC_ALPHA, GL_ONE, GL_ONE, GL_ONE);
	drawSnow(0, screenSize.y, settle * SNOW_ALPHA);
	engine.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);

	glMatrixMode(GL_TEXTURE);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);

	// --- Der graue Schleier ------------------------------------------------
	glDisable(GL_TEXTURE_2D);
	glBegin(GL_QUADS);
	glColor4d(0.62, 0.63, 0.60, settle * WASH_ALPHA);
	glVertex2i(0, 0);
	glVertex2i(screenSize.x, 0);
	glVertex2i(screenSize.x, screenSize.y);
	glVertex2i(0, screenSize.y);
	glEnd();

	// --- Die Einblendung ---------------------------------------------------
	// Ruhig, mit Schatten, wie ein Zeichengenerator sie zumischt.
	const Font::Options saved = p_font->getOptions();
	Font::Options options = saved;
	options.shadows = 1;
	p_font->setOptions(options);
	p_font->renderText("<< REW", Vec2i(OSD_X, OSD_Y), Vec4d(1.0, 1.0, 1.0, settle));
	p_font->setOptions(saved);

	glDisable(GL_TEXTURE_2D);
}
