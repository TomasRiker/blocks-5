#include "pch.h"
#include "hint.h"
#include "player.h"
#include "engine.h"
#include "font.h"
#include "texture.h"

namespace
{
	// Der Zettel im Grossen: das Bild misst 300x400 und der Text beginnt ein
	// Stueck innerhalb davon. Die Textur, in die beides zusammen gezeichnet
	// wird, ist eine Zweierpotenz - WebGL 1 kann mit anderen nur eingeschraenkt
	// umgehen, und das Blatt passt bequem hinein.
	const int NOTE_WIDTH = 300;
	const int NOTE_HEIGHT = 400;
	const int NOTE_TEXTURE_SIZE = 512;
	const int TEXT_LEFT = 35;
	const int TEXT_TOP = 45;
	const int TEXT_WIDTH = 230;

	// Der Schatten liegt fuenf Bildpunkte versetzt darunter.
	const int SHADOW_OFFSET = 5;
	const double SHADOW_ALPHA = 0.3;

	// Wie sich der Zettel aufrollt. ROLL_LENGTH ist der Anteil des Blattes, der
	// oben und unten eingerollt ist, solange es zufliegt; ROLL_TURNS, wie weit
	// es sich dabei einwickelt.
	//
	// Eine halbe Umdrehung ist die Grenze, und das ist keine Geschmacksfrage:
	// bis dahin wird jedes Stueck Papier auf dem Weg nach aussen dem Betrachter
	// naeher, die Rolle laesst sich also von hinten nach vorn zeichnen und
	// ueberdeckt sich richtig. Darueber hinaus kaeme das Ende wieder nach
	// hinten, und ohne Tiefenpuffer - den dieser Durchgang nicht hat - laege es
	// trotzdem obenauf.
	const double ROLL_LENGTH = 0.30;
	const double ROLL_TURNS = 0.45;

	// Wie fein. Die Rollen bekommen die Unterteilung, die flache Mitte braucht
	// keine: dort ist nichts zu kruemmen.
	const int ROLL_BANDS = 48;

	// Brennweite in Bildpunkten, fuer die Perspektive von Hand: was naeher am
	// Betrachter liegt, wird groesser. Ohne das waere die Rolle nur ein
	// gestauchter Streifen.
	const double PERSPECTIVE = 700.0;

	const double PI = 3.1415926535897932384626433832795;

	// Wie dunkel die vom Licht abgewandte Seite der Rolle wird. 1.0 ist die
	// Helligkeit des flachen Blattes, und dort faengt es auch an.
	const double SHADE_MIN = 0.30;

	// Wann sich der Zettel aufrollt und wie lange er dazu braucht, beides in
	// Logiktakten ab dem Betreten. Bei 20 Takten ist er auf 96% seiner Groesse -
	// er liegt also schon fast, wenn es losgeht.
	const int UNROLL_START = 20;
	const int UNROLL_END = 40;

	// Beim Verlassen geht es schneller zu, und das muss es auch: shownAlpha
	// faellt um 15% je Takt, nach zwanzig Takten waere vom Zettel nichts mehr da,
	// was sich noch einrollen koennte.
	const int ROLL_UP_SPEED = 3;

	// Ab wann der Zettel als angekommen gilt und auf ganze Bildpunkte gerundet
	// wird: ein halber Bildpunkt auf der Bildschirmdiagonalen von 800.
	const double SNAP_RESIDUAL = 0.5 / 800.0;

	// Ein Punkt auf dem Papier. py laeuft von 0 (Oberkante) bis NOTE_HEIGHT.
	struct NotePoint
	{
		double y;       // Lage im Bild, von der Mitte des Zettels aus
		double depth;   // wie weit vor der Blattebene, also naeher am Betrachter
		double shade;   // wie hell das Papier hier steht
	};

	NotePoint rollPoint(double py,
						double unroll)
	{
		NotePoint p;
		p.y = py - 0.5 * NOTE_HEIGHT;
		p.depth = 0.0;
		p.shade = 1.0;

		const double rolled = ROLL_LENGTH * NOTE_HEIGHT * (1.0 - unroll);
		if(rolled < 1.0) return p;   // ausgerollt: nichts mehr zu kruemmen

		const double thetaMax = ROLL_TURNS * 2.0 * PI;
		const double radius = rolled / thetaMax;

		// Bogenlaenge vom Knick aus, an dem das Papier von der Ebene abhebt.
		double s = 0.0;
		double direction = 0.0;
		if(py < rolled)                      { s = rolled - py;                     direction = -1.0; }
		else if(py > NOTE_HEIGHT - rolled)   { s = py - (NOTE_HEIGHT - rolled);     direction = 1.0; }
		else return p;

		const double theta = s / radius;
		const double edge = direction * (0.5 * NOTE_HEIGHT - rolled);
		p.y = edge + direction * radius * sin(theta);
		p.depth = radius * (1.0 - cos(theta));
		p.shade = SHADE_MIN + (1.0 - SHADE_MIN) * (0.5 + 0.5 * cos(theta));
		return p;
	}
}

Hint::Hint(Level& level,
		   const Vec2i& position,
		   const std::string& text) : Object(level, 2)
{
	warpTo(position);
	flags = OF_FIXED | OF_TRANSPORTABLE | OF_COLLECTABLE;
	this->text = text;
	alpha = shownAlpha = 0.0;
	unroll = 0.0;
	activeTicks = 0;
	noteTexture = 0;
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
	// Der uebliche Weg ist onRemove(): Level::removeObject() ruft es, und
	// removeOldObjects() ist die einzige Stelle, die ein Object je loescht -
	// ein Zettel kann also nicht verschwinden, ohne dass es gelaufen waere.
	// Hier steht es trotzdem noch einmal, damit die Rueckgabe nicht an einem
	// einzelnen Haken haengt.
	//
	// Der Zugriff auf die Engine ist dabei sicher, weil releaseNoteTexture()
	// vorher aussteigt, wenn gar keine Textur geliehen ist: eine hat nur, wer
	// in einem laufenden Level gezeichnet hat, und dessen Level gehoert einer
	// lokalen Variablen von main() - die faellt vor dem statischen Engine.
	//
	// Im Browser laeuft dieser Destruktor ohnehin nie: dort kehrt mainLoop()
	// nicht zurueck. Zurueckgegeben wird die Textur da oben in onUpdate(),
	// sobald der Zettel unsichtbar ist, und in onRemove() beim Levelwechsel -
	// beides laeuft im Spiel und nicht beim Herunterfahren.
	releaseNoteTexture();
}

void Hint::onRemove()
{
	Object::onRemove();
	releaseNoteTexture();
}

void Hint::releaseNoteTexture()
{
	if(!noteTexture) return;
	Engine::inst().releaseOffscreenTexture(noteTexture);
	noteTexture = 0;
	bakedText = "";
}

void Hint::updateSprites()
{
	// Zettelobjekt
	sprites.add(Vec2i(96, 288));
}

void Hint::bakeNote()
{
	Engine& engine = Engine::inst();

	const std::string wanted = p_font->adjustText(localizeString(text), TEXT_WIDTH);
	if(noteTexture && wanted == bakedText) return;

	// Eine eigene Textur, keine gemeinsame: beim Schritt von einem Zettel auf
	// den nachbarn sind beide zu sehen, und der eine darf nicht in das Blatt
	// hineinzeichnen, aus dem der andere gerade liest.
	const Vec2i size(NOTE_TEXTURE_SIZE, NOTE_TEXTURE_SIZE);
	const uint target = noteTexture ? noteTexture : engine.acquireOffscreenTexture(size);
	if(!target) return;   // kein Bildpuffer: dann eben ohne, siehe onRender()

	if(!engine.beginRenderToTexture(target, size))
	{
		if(!noteTexture) engine.releaseOffscreenTexture(target);
		return;
	}

	GLfloat oldClear[4];
	glGetFloatv(GL_COLOR_CLEAR_VALUE, oldClear);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glClearColor(oldClear[0], oldClear[1], oldClear[2], oldClear[3]);

	// Der Alphakanal muss stimmen, denn die Textur wird gleich selbst wieder
	// gemischt: die Farbe kommt gewichtet an (GL_SRC_ALPHA), das Alpha aber
	// ungewichtet (GL_ONE). Was dabei entsteht, ist vormultipliziert - und
	// genau so wird es unten auch wieder gezeichnet.
	engine.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	p_sprite->bind();
	engine.renderSprite(Vec2i(0, 0), Vec2i(0, 0), Vec2i(NOTE_WIDTH, NOTE_HEIGHT), Vec4d(1.0));
	p_font->renderText(wanted, Vec2i(TEXT_LEFT, TEXT_TOP), Vec4d(1.0));

	engine.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
	engine.endRenderToTexture();

	noteTexture = target;
	bakedText = wanted;
}

void Hint::renderNoteMesh(const Vec4d& color,
						  double unroll) const
{
	// Erst die flache Mitte, dann die beiden Rollen von ihrem Knick nach
	// aussen: das ist die Reihenfolge von hinten nach vorn. Ohne Tiefenpuffer
	// ist sie die einzige, die stimmt.
	const double u = static_cast<double>(NOTE_WIDTH) / NOTE_TEXTURE_SIZE;
	const double rolled = ROLL_LENGTH * NOTE_HEIGHT * (1.0 - unroll);
	const double flatTop = (rolled < 1.0) ? 0.0 : rolled;
	const double flatBottom = NOTE_HEIGHT - flatTop;

	// [von, bis] auf dem Papier, und wie fein.
	double sections[3][2];
	int steps[3];
	sections[0][0] = flatTop;    sections[0][1] = flatBottom;   steps[0] = 1;
	sections[1][0] = flatTop;    sections[1][1] = 0.0;          steps[1] = ROLL_BANDS;
	sections[2][0] = flatBottom; sections[2][1] = NOTE_HEIGHT;  steps[2] = ROLL_BANDS;

	// Ein Dreiecksstreifen und nicht GL_QUAD_STRIP: den kennt WebGL nicht, und
	// der Web-Build reicht den Modus unveraendert weiter (WebBuild/gl_immediate.cpp).
	// Fuer eine Bahn aus Vierecken ist beides dasselbe.
	for(int section = 0; section < 3; section++)
	{
		const double from = sections[section][0];
		const double to = sections[section][1];
		if(from == to) continue;

		glBegin(GL_TRIANGLE_STRIP);
		for(int k = 0; k <= steps[section]; k++)
		{
			const double py = from + (to - from) * k / steps[section];
			const NotePoint np = rollPoint(py, unroll);

			// Perspektive von Hand: was naeher liegt, wird groesser.
			const double f = PERSPECTIVE / (PERSPECTIVE - np.depth);
			const double x = 0.5 * NOTE_WIDTH * f;
			const double y = np.y * f;
			// Umgekehrt: gezeichnet wurde mit (0,0) links OBEN, und eine
			// Textur faengt unten an. In der Textur steht der Zettel also auf
			// dem Kopf, genau wie das Spielbild im Bildpuffer.
			const double t = 1.0 - py / NOTE_TEXTURE_SIZE;

			// Vormultipliziert: die Farbe traegt das Alpha schon in sich, also
			// muss der Anstrich es mitnehmen.
			const double b = np.shade * color.a;
			glColor4d(color.r * b, color.g * b, color.b * b, color.a);
			glTexCoord2d(0.0, t); glVertex2d(-x, y);
			glTexCoord2d(u,   t); glVertex2d( x, y);
		}
		glEnd();
	}
}

void Hint::renderNote(const Vec4d& color,
					  double unroll) const
{
	Engine& engine = Engine::inst();

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, noteTexture);

	// Diese Textur kommt nicht aus Texture::bind(), also steht dort noch die
	// Pixelmatrix des letzten Bildes. Hier wird in 0..1 gerechnet.
	glMatrixMode(GL_TEXTURE);
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);

	// Vormultipliziert mischen, weil die Textur so entstanden ist.
	engine.setBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	// Der Schatten ist dasselbe Papier in Schwarz, ein Stueck versetzt.
	glPushMatrix();
	glTranslated(SHADOW_OFFSET, SHADOW_OFFSET, 0.0);
	renderNoteMesh(Vec4d(0.0, 0.0, 0.0, color.a * SHADOW_ALPHA), unroll);
	glPopMatrix();

	renderNoteMesh(color, unroll);

	engine.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);

	glMatrixMode(GL_TEXTURE);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);

	// So aufgeraeumt hinterlassen, wie Texture::unbind() es taete: gleich
	// darauf zeichnet das Level den Blitz, und der will keine Textur.
	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_TEXTURE_2D);
}

void Hint::renderNoteFlat(const Vec4d& color) const
{
	// Ohne Bildpuffer gibt es keine Textur, in die sich Zettel und Text
	// zusammenzeichnen liessen. Dann eben beides hintereinander - unter
	// derselben Matrix, damit die Schrift wenigstens mitfliegt und nicht am
	// Ende aus dem Nichts erscheint.
	Engine& engine = Engine::inst();
	const Vec2i corner(-NOTE_WIDTH / 2, -NOTE_HEIGHT / 2);

	p_sprite->bind();
	engine.renderSprite(corner + Vec2i(SHADOW_OFFSET, SHADOW_OFFSET), Vec2i(0, 0),
						Vec2i(NOTE_WIDTH, NOTE_HEIGHT),
						Vec4d(0.0, 0.0, 0.0, color.a * SHADOW_ALPHA));
	engine.renderSprite(corner, Vec2i(0, 0), Vec2i(NOTE_WIDTH, NOTE_HEIGHT), color);
	p_font->renderText(p_font->adjustText(localizeString(text), TEXT_WIDTH),
					   corner + Vec2i(TEXT_LEFT, TEXT_TOP), color);
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
		double s = i;

		// Layer 43 ist die Vorschau im Leveleditor: fertig aufgeklappt, mittig.
		// Das ist eine Anzeigesache und darf targetPosition nicht veraendern -
		// sonst zeigt der Zettel im Spiel hinterher woandershin.
		//
		// Aufgerollt kommt der Zettel nur, wenn das Bild ein Blatt Papier ist -
		// der Weltraum-Skin zeigt eine Anzeigetafel, und die rollt sich nicht.
		// 1.0 heisst flach: dann faellt in renderNoteMesh() beides weg bis auf
		// das eine Viereck.
		Vec2i target = targetPosition;
		double shownUnroll = level.isHintScroll() ? unroll : 1.0;
		if(layer == 43) a = 0.85, r = 0.0, i = 1.0, s = 1.0, target = Vec2i(320, 200), shownUnroll = 1.0;

		// Angekommen heisst exakt angekommen. shownAlpha naehert sich 0.85 und
		// kommt nie an, also bleibt i knapp unter 1: der Massstab knapp
		// darunter, der Winkel knapp ueber 0, die Lage um Bruchteile daneben.
		// Jedes davon legt die gebackene Textur zwischen die Bildpunkte, und
		// weil sie mit GL_LINEAR gelesen wird, mischt die Karte jedes Texel aus
		// zweien - der Text wird weich und schlecht zu lesen.
		//
		// Sobald der Rest weniger als einen halben Bildpunkt ausmacht, wird
		// deshalb gerundet. SNAP_RESIDUAL ist genau das: ein halber Bildpunkt,
		// geteilt durch den laengsten Hebel im Spiel - die Bildschirmdiagonale,
		// an der sowohl der Restweg als auch die Restdrehung angreifen.
		//
		// Danach steht alles ganzzahlig: targetPosition ist ein Vec2i, der
		// Massstab genau 1, der Winkel genau 0, und die Ecken der Bahn in
		// renderNoteMesh() liegen ohnehin auf ganzen Bildpunkten. Jedes Texel
		// trifft dann genau einen Bildpunkt und wird in dessen Mitte gelesen.
		if(1.0 - i < SNAP_RESIDUAL) i = 1.0, s = 1.0, r = 0.0;

		if(a > 1.0 / 255.0)
		{
			// Zettel und Text stecken zusammen in einer Textur, damit die
			// Schrift mitdreht und sich mit aufrollt. Sie entsteht beim ersten
			// Bild und immer dann neu, wenn der Text ein anderer ist - im
			// Editor also bei jedem Tastendruck.
			bakeNote();

			glPushMatrix();
			Vec2i p = -getShownPositionInPixels();
			glTranslated(p.x, p.y, 0.0);

			Vec4d realColor(color.r, color.g, color.b, color.a * a);

			glPushMatrix();
			Vec2d sp = (1.0 - i) * static_cast<Vec2d>(getShownPositionInPixels()) + i * static_cast<Vec2d>(target);
			glTranslated(sp.x, sp.y, 0.0);
			glScaled(s, s, 1.0);
			glRotated(r, 0.0, 0.0, 1.0);

			if(noteTexture) renderNote(realColor, shownUnroll);
			else renderNoteFlat(realColor);

			glPopMatrix();
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
	if(shownAlpha <= 1.0 / 255.0)
	{
		shownAlpha = 0.0;
		releaseNoteTexture();
	}

	// Das Aufrollen laeuft nach der Uhr und nicht nach shownAlpha: das naehert
	// sich seinem Ziel nur an und kaeme nie ganz an, der Zettel bliebe also
	// fuer immer ein wenig eingerollt.
	if(playerIsHere) { if(activeTicks < UNROLL_END) activeTicks++; }
	else             { activeTicks = max(0, activeTicks - ROLL_UP_SPEED); }
	unroll = clamp(static_cast<double>(activeTicks - UNROLL_START) /
				   (UNROLL_END - UNROLL_START), 0.0, 1.0);
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
