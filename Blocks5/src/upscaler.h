#ifndef _UPSCALER_H
#define _UPSCALER_H

// Wie das intern gerenderte 640x480-Bild auf den Bildschirm kommt.
//
// Jeder Filter ist eine eigene Klasse: er kennt seinen Namen in der config.xml,
// zeichnet selbst, bringt seine eigenen Einstellungen mit und weiss, ob er das
// Bild verzieht. Die Engine besitzt einen von jeder Sorte, waehlt einen davon
// aus und muss sonst nichts ueber sie wissen - insbesondere nicht, dass es eine
// gewoelbte Scheibe gibt.

// Alles, was ein Filter zum Zeichnen braucht. Es gehoert samt und sonders der
// Engine: Bildpuffer, Textur und Vertexpuffer entstehen und vergehen mit dem
// GL-Kontext, nicht je Filter.
struct PresentContext
{
	Vec2i rectPosition;   // linke untere Ecke des Zielrechtecks im Fenster
	Vec2i rectSize;       // seine Groesse; das Seitenverhaeltnis stimmt schon
	Vec2i displaySize;    // Fenstergroesse, fuer die Clipkoordinaten
	Vec2i frameSize;      // das gerenderte Bild, immer 640x480
	Vec2i textureSize;    // Zweierpotenz; frameSize liegt links unten darin
	uint textureID;
	uint vertexBuffer;    // WebGL verbietet Vertexdaten aus dem Anwendungsspeicher
};

// Ein uebersetztes Praesentierprogramm. Den Vertexshader teilen sich alle - er
// steht als einziger Text in upscaler.cpp -, die Fragmentquelle bringt jeder
// Filter selbst mit. Hier stehen nur die vier Uniforms, die *jeder* von ihnen
// hat; wer mehr braucht, holt sie sich selbst. Deshalb gibt es hier keine
// einzige, die bei der Haelfte der Filter auf -1 steht.
struct PresentProgram
{
	PresentProgram();

	// p_name steht nur in der Fehlermeldung. Ein zweiter Aufruf raeumt das alte
	// Programm vorher weg: im Browser kann der GL-Kontext verlorengehen.
	bool create(const char* p_fragmentSource, const char* p_name);
	void destroy();
	bool isLinked() const { return id != 0; }

	// Programm binden und die vier gemeinsamen Uniforms setzen. Danach kann ein
	// Filter seine eigenen nachreichen, vor drawQuad().
	void use(const PresentContext& context) const;

	// Das Zielrechteck als zwei Dreiecke zeichnen und den Zustand aufraeumen.
	void drawQuad(const PresentContext& context) const;

	// Eine Uniform setzen, wenn es sie gibt. Ein Filter, der eine Zeile seines
	// Shaders auskommentiert, bekommt sonst GL_INVALID_OPERATION statt nichts.
	static void setUniform(int location, double value);

	uint id;
	// Je eine eigene Zeile: Tools/verify.py sucht "Typ Name;" und uebersaehe
	// eine Sammeldeklaration - und damit auch eine, die der Konstruktor
	// vergisst. Genau so blieb "convergence" ungesetzt.
	int decal;
	int textureSize;
	int frameSize;
	int prescale;
};

class Upscaler
{
public:
	Upscaler();
	// Raeumt *keinen* GL-Zustand ab: der Destruktor laeuft, wenn der Kontext
	// laengst weg ist. Dafuer gibt es destroyGL().
	virtual ~Upscaler();

	// Der Name in der config.xml, im Optionsdialog, im Protokoll und im
	// Testhaken - ein Dateiformat, keine Beschriftung. Die Beschriftung steht
	// als $ID an den Label-Elementen in data/options.xml.
	virtual const char* getName() const = 0;

	// GL-Zustand anlegen und abbauen, beides nur mit stehendem Kontext und
	// beides mehrfach erlaubt. Wer keinen braucht, laesst es beim Basisfall.
	virtual bool createGL() { return true; }
	virtual void destroyGL() {}

	// Kann dieser Filter auf dieser Maschine zeichnen? Ohne uebersetztes
	// Programm nein. Ein Bildpuffer ist keine Bedingung, sondern eine
	// Voraussetzung: ohne ihn ruft die Engine createGL() gar nicht erst.
	virtual bool isAvailable() const { return true; }

	// GL_NEAREST oder GL_LINEAR fuer die Bildpuffertextur.
	virtual GLint getTextureFilter() const = 0;

	// Nur "Scharf" braucht eine ganzzahlige Vergroesserungsstufe: bei einem
	// krummen Faktor verdoppelt Nearest manche Quellpixel und andere nicht.
	virtual bool wantsIntegerScale() const { return false; }

	// Das Bild auf den Schirm bringen. Texturfilter, Matrizen und der schwarze
	// Hintergrund stehen schon, die Textur ist gebunden. Die Vorgabe ist das
	// Viereck der festen Funktionsstufe - "Scharf" und "Weich" unterscheiden
	// sich nur im Texturfilter und zeichnen beide so.
	virtual void present(const PresentContext& context);

	// Dieselbe Abbildung wie im eigenen Shader, in beide Richtungen; die
	// Koordinaten laufen von -1 bis 1 ab der Bildmitte. Wer das Bild nicht
	// verzieht, gibt zurueck, was er bekommen hat. Beide sitzen auf dem
	// 20-ms-Takt und auf jedem aufgezeichneten Bild - ein virtueller Aufruf ist
	// in Ordnung, eine Suche nach Namen waere es nicht.
	virtual Vec2d warpToSource(const Vec2d& p) const { return p; }
	virtual Vec2d warpToOutput(const Vec2d& s) const { return s; }

	// Verzieht dieser Filter das Bild gerade wirklich? Der Testhaken meldet es,
	// damit ein Test bemerkt, dass seine Fensterkoordinaten nicht mehr stimmen.
	virtual bool distortsCursor() const { return false; }

	// Die eigenen Einstellungen unter <Config> lesen und dorthin schreiben. Der
	// Filter legt sein Element selbst an - deshalb steht der Elementname in
	// u_*.cpp und nicht in der Engine, und deshalb sieht Tools/verify.py beide
	// Haelften beieinander. Laeuft ohne GL-Kontext, und darf mehrfach kommen:
	// loadConfig() ruft auch der Abbrechen-Knopf des Optionsdialogs, mitten im
	// Spiel. Also nur lesen, was dasteht, und nichts zurueckstellen.
	virtual void loadConfig(TiXmlElement* p_config);
	virtual void saveConfig(TiXmlElement* p_config);
};

#endif
