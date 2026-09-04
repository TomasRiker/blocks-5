#include "pch.h"
#include "u_crt.h"
#include "glextensions.h"

/* Ein Bildschirm aus den neunziger Jahren, nachgebaut im Praesentierdurchgang.

   Der Filter rekonstruiert nichts - anders als xBR, das hier zweimal versucht
   und zweimal verworfen wurde. Er legt eine Darstellung ueber das Bild, so wie
   es gezeichnet wurde, und ist damit ehrlich darueber, was er tut. Nebenbei ist
   er stabil: xBR bestand aus step()-Entscheidungen an Schwellen, und ein
   Farbunterschied von 1/255 unter einem halbdurchsichtigen Dialog kippte sie,
   worauf 1% der Pixel um bis zu 154 sprangen. Hier gibt es keine Schwelle und
   keine Kantenerkennung, nur glatte Funktionen der Quellfarbe und der Position:
   bewegt sich die Eingabe um 1, bewegt sich die Ausgabe um etwa 1.

   WELCHER BILDSCHIRM? Das ist die eigentliche Frage, und sie steckt in genau
   einer Zahl, SCANLINE_PERIOD.

   Sichtbare Luecken zwischen den Zeilen sind ein Artefakt von 240p: eine
   Konsole zeichnete 240 Zeilen in ein 480-Zeilen-Raster, dazwischen blieb das
   Leuchtmittel dunkel. Ein VGA-Monitor mit 640x480 zeichnete alle 480 Zeilen,
   und ihre Strahlprofile ueberlappten sich - da war keine Luecke. Blocks 5 ist
   ein 640x480-Windows-Spiel; sein ehrliches Vorbild ist der beige 15-Zoll-
   Monitor, nicht der Fernseher mit der Konsole daran. SCANLINE_PERIOD = 1.0
   bildet das ab. 2.0 tut so, als kaemen 240 Zeilen an, und liefert den
   Konsolen-Look, den die meisten Leute heute mit "Roehre" meinen.

   Das ist keine Kleinigkeit der Optik: bei Periode 1 und einem Fenster mit
   doppelter Groesse liegen beide Ausgabezeilen gleich weit von der Zeilenmitte
   entfernt, also ist ueberhaupt kein Streifen zu sehen - physikalisch richtig
   und als Effekt wertlos. Erst Periode 2 erzeugt bei 2x eine sichtbare
   Struktur. Der Regler "Zeilen" blendet den Effekt auf, die Periode entscheidet,
   welchen.

   AUFLOESUNG. Fast alle spielen mit genau 2x (1280x960): auf 1080p und 1440p
   gibt getDefaultWindowSize() 2x, erst 1600p bekommt 3x. Zwei Ausgabepixel je
   Quellpixel sind wenig. Deshalb sitzt die Maske im *Ausgabe*-Raster und nicht
   im Quellraster - eine echte Lochmaske gehoert zur Glasscheibe und aendert
   sich nicht, wenn man die Aufloesung umschaltet. MASK_PITCH ist in
   Ausgabepixeln angegeben und bleibt bei jedem Faktor gleich fein.

   VERZERRUNG. Die Scheibe war gewoelbt. Die Abbildung geht vom Ausgabepixel zum
   Quellpixel, denn genau in die Richtung fragt ein Fragmentshader:

       src.x = out.x * (1 + a*out.y^2)
       src.y = out.y * (1 + b*out.x^2)

   mit out und src in -1..1 von der Bildmitte aus. Die Kantenmitten bleiben
   stehen, nur die Ecken wandern nach aussen - deshalb sieht es richtig aus und
   es geht keine Flaeche an den Seitenmitten verloren. Dieselbe Formel steht in
   engine.cpp noch einmal in C++, weil die Mausposition durch dieselbe Abbildung
   muss; die Umkehrung dazu ist eine Fixpunktiteration. Wer hier etwas aendert,
   aendert es dort mit.

   Kein #version: 110 auf dem Desktop, 100 unter GLSL ES, uebersetzt als beides.
   Keine Feldkonstanten und keine Schleifen mit variabler Grenze - GLSL ES 1.00
   kennt beides nicht. */

/* Die Woelbung, einmal definiert und zweimal benutzt: als Text im Shader und
   als double in engine.cpp (warpToSource/warpToOutput). */

#define CRT_CURVE_X 0.10
#define CRT_CURVE_Y 0.13
/* Zeilenperioden je Sekunde bei Flimmern auf Anschlag; siehe CRAWL_JITTER. */
#define CRT_CRAWL_SPEED 1.2
/* Nach so vielen Sekunden wiederholt sich das Flimmern exakt. Steht ebenfalls
   zweimal da - im Shader und in present(), das die Uhr darauf kuerzt. */
#define CRT_FLICKER_CYCLE 8.0
#define CRT_STR2(x) #x
#define CRT_STR(x) CRT_STR2(x)

static const double crtCurveX = CRT_CURVE_X;
static const double crtCurveY = CRT_CURVE_Y;
static const double crtCrawlSpeed = CRT_CRAWL_SPEED;

static const char* p_crtFragmentShader =
	"#ifdef GL_ES\n"
	"#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
	"precision highp float;\n"
	"#else\n"
	"precision mediump float;\n"
	"#endif\n"
	"#endif\n"

	/* ------------------------------------------------------------------ */
	/* Stellschrauben. Alles, was den Charakter ausmacht, steht hier oben. */
	/* ------------------------------------------------------------------ */

	/* Welche Roehre. 1.0 = VGA-Monitor, alle 480 Zeilen, keine Luecken (bei 2x
	   also praktisch keine sichtbaren Streifen). 2.0 = so tun, als kaemen 240
	   Zeilen: der Konsolen-Look, sichtbar auch bei 2x. Zwischenwerte sind
	   erlaubt, sehen aber wie ein Fehler aus. */
	"const float SCANLINE_PERIOD = 2.0;\n"

	/* Breite des Elektronenstrahls, in Zeilenabstaenden. Kleiner = schmalerer
	   Strahl = tiefere und haertere Streifen. 0.5 ist kraeftig, 0.8 sanft. */
	"const float BEAM_WIDTH = 0.55;\n"

	/* Die Maske, in AUSGABEPIXELN je RGB-Tripel. 3.0 ist eine echte
	   Streifenmaske: ein Pixel rot, eines gruen, eines blau. Bei 2x schlaegt
	   das mit dem Quellraster (Periode 2) zu einem Muster mit Periode 6
	   zusammen; 2.0 oder 4.0 sind dort ruhiger. */
	"const float MASK_PITCH = 3.0;\n"
	/* Wie dunkel die gerade nicht angeregten Streifen werden. 0 = keine Maske,
	   1 = die beiden anderen Kanaele ganz aus (viel zu viel). 0.3 ist deutlich
	   sichtbar, ohne dass Farben kippen. */
	"const float MASK_STRENGTH = 0.30;\n"

	/* Halation: Licht streut in der Glasscheibe und kommt als weicher Hof
	   zurueck. Nur was heller als die Schwelle ist, leuchtet - dunkle Flaechen
	   bleiben scharf, und genau diese Unsymmetrie unterscheidet das von einer
	   Weichzeichnung. Radius in Quellpixeln.

	   BLOOM_STRENGTH ist der Wert bei Regler auf Anschlag; der Regler
	   (Uniform Bloom) skaliert ihn. Auf 0 gesetzt faellt der ganze Block beim
	   Uebersetzen weg - siehe unten. */
	"const float BLOOM_STRENGTH  = 0.38;\n"
	"const float BLOOM_THRESHOLD = 0.22;\n"
	"const float BLOOM_RADIUS    = 2.5;\n"   /* innerer Ring, Quellpixel */
	"const float BLOOM_OUTER     = 2.6;\n"   /* aeusserer Ring als Vielfaches davon */

	/* Waagerechte Bandbreite. Das Videosignal war analog und begrenzt, deshalb
	   war eine Roehre quer weicher als senkrecht. In Bruchteilen eines
	   Quellpixels; 0 = so scharf wie "Scharf, angepasst". */
	"const float SOFTNESS = 0.35;\n"

	/* Konvergenz. Eine Farbroehre hat drei Elektronenstrahlen, und die treffen
	   die Maske nie an genau derselben Stelle: in der Mitte wird sie justiert,
	   nach aussen laeuft es auseinander, weil die Ablenkung dort am groessten
	   ist. Sichtbar wird das als roter und blauer Saum an senkrechten Kanten,
	   in der Mitte gar nicht und am Rand am staerksten.

	   Das ist ausdruecklich *keine* chromatische Aberration - die entsteht in
	   einer Linse, weil Glas Wellenlaengen verschieden bricht, und eine Roehre
	   hat keine. Hier liegen schlicht drei Bilder nebeneinander.

	   Der Wert ist, bei Regler auf Anschlag, die Verschiebung *je Strahl* am
	   linken und rechten Bildrand, in Quellpixeln; Rot und Blau laufen
	   gegeneinander, der sichtbare Saum ist also doppelt so breit. Ein gut
	   eingestelltes Geraet blieb darunter, ein muedes Billiggeraet kam in den
	   Ecken auf ein bis zwei Triaden, was hier ein bis zwei Quellpixeln
	   entspricht. Auf 0 gesetzt faellt der Block beim Uebersetzen weg, wie bei
	   der Halation. */
	"const float CONVERGENCE_MAX = 1.6;\n"

	/* Woelbung bei Regler auf Anschlag. Lottes nimmt 1/32 und 1/24; hier darf
	   es weiter gehen, der Regler steht ja normalerweise nicht am Anschlag.
	   Die Zahlen kommen aus den Makros unten - der Shader und die
	   Mausumrechnung in engine.cpp muessen dieselbe Woelbung rechnen, und ein
	   Zahlenpaar, das man an zwei Stellen pflegen muss, geht irgendwann
	   auseinander. Der Praeprozessor setzt hier denselben Text ein, den C++
	   als double sieht. */
	"const float CURVE_X = " CRT_STR(CRT_CURVE_X) ";\n"
	"const float CURVE_Y = " CRT_STR(CRT_CURVE_Y) ";\n"
	/* Abgerundete Ecken, in Bruchteilen der halben Bildhoehe. 0 = rechteckig. */
	"const float CORNER_RADIUS = 0.10;\n"
	/* Randabdunklung. 0 = aus. */
	"const float VIGNETTE = 0.22;\n"

	/* Flimmern, wie es ein alter Fernseher hatte. Es sind zwei Dinge, und sie
	   haben je einen eigenen Regler, weil man sie durchaus getrennt haben will:

	   "Flimmern" (Uniform Flicker) ist das schnelle Zittern der *Helligkeit* -
	   drei Schwingungen um 12, 19 und 29 Hz, die sich staendig neu ueberlagern
	   und nie in ein Muster fallen -, dazu viel schwaecher das Netzbrummen: ein
	   breites, dunkles Band, das langsam durchs Bild wandert, weil die
	   Netzfrequenz gegen die Bildfrequenz schwebt.

	   "Zeilenflimmern" (Uniform ScanFlicker) betrifft die *Lage* der Zeilen:
	   sie wandern langsam nach unten und zittern dabei. Siehe CRAWL_JITTER.

	   Alle Anteile haben Mittelwert null, kosten also keine Helligkeit, und
	   alle haengen nur an der Uhr, nicht am vorigen Bild - der Fehler, an dem
	   xBR gescheitert ist, kann hier nicht auftreten. Werte bei Regler auf
	   Anschlag; wem das Band nicht gefaellt, setzt HUM_DEPTH auf 0. Beide
	   Tiefen waren einmal um die Haelfte groesser (0.055 und 0.022); auf
	   Dauer war das zu unruhig, um davor zu sitzen. */
	"const float FLICKER_DEPTH = 0.0367;\n"  /* schnelles Helligkeitszittern */
	"const float HUM_DEPTH     = 0.0147;\n"  /* Tiefe des wandernden Bandes */
	"const float HUM_BARS      = 0.75;\n"    /* wie viele Baender ins Bild passen */
	"const float HUM_ROLLS     = 3.0;\n"     /* Durchlaeufe je FLICKER_CYCLE */
	/* Nach so vielen Sekunden wiederholt sich das Flimmern exakt. Alle
	   Frequenzen unten sind ganze Vielfache davon, deshalb ist der Uebergang
	   nahtlos und die Uhr darf bei jedem Durchlauf von vorn anfangen - sonst
	   wuerde float irgendwann grob. Aus dem Makro oben, weil present() die Uhr
	   auf denselben Wert kuerzen muss. */
	"const float FLICKER_CYCLE = " CRT_STR(CRT_FLICKER_CYCLE) ";\n"

	/* Das Zeilenbild steht nicht still. Auf einer echten Roehre wandert es
	   langsam nach unten - das Zeilenkriechen, weil Zeilen- und Bildfrequenz
	   nie exakt ins Verhaeltnis gehen - und zittert dabei ein wenig. Ohne das
	   sehen die Streifen wie aufgemalt aus.

	   CRAWL_SPEED steht in Zeilenperioden je Sekunde bei Regler auf Anschlag
	   und wird als einziger Wert auf der CPU gerechnet: die Phase muss aus der
	   ungekuerzten Uhr kommen, sonst spraenge sie bei jedem Umlauf um
	   fract(Flicker * Geschwindigkeit) einer Periode. Deshalb steht die Zahl
	   unten noch einmal als Makro, so wie die Woelbung.
	   CRAWL_JITTER ist das schnelle Zittern der Phase, in Perioden - das
	   laeuft im Shader, weil es eine Schwingung mit ganzzahliger Frequenz ist
	   und deshalb von allein nahtlos umlaeuft. */
	"const float CRAWL_JITTER = 0.05;\n"

	/* Gamma. Eine Roehre hatte ungefaehr 2.4; gerechnet wird dazwischen in
	   linearem Licht, sonst wird aus dem Hof grauer Dunst. */
	"const float GAMMA_IN  = 2.4;\n"
	"const float GAMMA_OUT = 2.2;\n"
	/* Reine Geschmackssache. Maske und Streifen nehmen zwar Licht weg, aber das
	   rechnet der Shader unten selbst wieder heraus (MASK_AVG und SCAN_AVG),
	   und zwar aus den Konstanten - wer oben etwas verstellt, bekommt die
	   Helligkeit also von allein zurueck und muss hier nichts nachziehen.
	   1.0 = so hell wie ohne Filter. */
	"const float BRIGHTNESS = 1.0;\n"

	/* ------------------------------------------------------------------ */

	"uniform sampler2D decal;\n"
	"uniform vec2 TextureSize;\n"   /* ganze Textur, Zweierpotenz */
	"uniform vec2 FrameSize;\n"     /* der benutzte Teil davon, 640x480 */
	"uniform vec2 Prescale;\n"      /* wie bei sharp-fit: ganzzahlig, >= 1 */
	"uniform float Scanline;\n"     /* Regler 0..1 */
	"uniform float Curvature;\n"    /* Regler 0..1 */
	"uniform float Bloom;\n"        /* Regler 0..1 */
	"uniform float Flicker;\n"      /* Regler 0..1, Helligkeit */
	"uniform float ScanFlicker;\n"  /* Regler 0..1, Lage der Zeilen */
	"uniform float Convergence;\n"  /* Regler 0..1, Farbsaeume am Rand */
	"uniform float Time;\n"         /* Sekunden, 0 .. FLICKER_CYCLE */
	"uniform float ScanPhase;\n"    /* Zeilenkriechen, 0..1 Perioden */
	"varying vec2 texCoord;\n"

	/* Ein Texel holen, mit derselben stueckweise linearen Umrechnung wie
	   sharp-fit, nur um SOFTNESS aufgeweicht. Die Rampe an der Pixelgrenze ist
	   dort 1/N Quellpixel breit; hier wird sie breiter, und das ist genau die
	   begrenzte Bandbreite. Senkrecht bleibt es scharf - eine Roehre war quer
	   weich und laengs nicht. */
	"vec3 fetch(vec2 uv)\n"
	"{\n"
	"    vec2 texel = uv * FrameSize;\n"
	"    vec2 base  = floor(texel);\n"
	"    vec2 d     = (texel - base) - 0.5;\n"
	"    vec2 soft  = vec2(1.0 + SOFTNESS * Prescale.x, 1.0);\n"
	"    vec2 n     = max(Prescale / soft, vec2(1.0));\n"
	"    vec2 flat_ = 0.5 - 0.5 / n;\n"
	"    vec2 f     = (d - clamp(d, -flat_, flat_)) * n + 0.5;\n"
	"    vec2 p     = clamp(base + f, vec2(0.5), FrameSize - 0.5);\n"
	"    return texture2D(decal, p / TextureSize).rgb;\n"
	"}\n"

	/* Der Hof braucht die scharfe Umrechnung aus fetch() nicht - ein weiches
	   Bild von einem weichen Bild -, das spart acht Mal die Rampenrechnung.

	   Gemittelt wird aber in *linearem* Licht, und zwar je Griff. Die erste
	   Fassung hat erst gemittelt, dann umgerechnet, dann die Schwelle
	   angewandt, und damit gab es ueberhaupt keinen sichtbaren Hof: der Ring um
	   eine helle Stelle ist eine Mischung aus hell und dunkel, und pow() auf
	   diesen Mittelwert drueckt ihn weit unter die Schwelle. Gemessen bewegte
	   der Regler von 0 auf 100 dadurch 0.6% der Bildpunkte. Der Mittelwert
	   gehoert ins lineare Licht, wo Licht sich tatsaechlich addiert.

	   Umgerechnet wird mit x*x statt pow(x, GAMMA_IN) - Gamma 2.0 statt 2.4.
	   Fuer einen weichen Hof ist der Unterschied bedeutungslos, und es kostet
	   eine Multiplikation statt eines pow(); acht davon je Ausgabepixel waeren
	   sonst der teuerste Posten im ganzen Shader. */
	"vec3 toLinear(vec3 c) { return pow(max(c, vec3(0.0)), vec3(GAMMA_IN)); }\n"

	/* Der Rand des Rasters, als Abstandsfunktion eines abgerundeten Rechtecks:
	   1 innerhalb, 0 ausserhalb, dazwischen eine Kante von zwei edge breit.

	   wc ist der Quellpunkt, den *dieser Kanal* liest, nicht der Ausgabepunkt.
	   Darauf kommt es an: eine Farbroehre malt drei Raster, und wenn das rote
	   enger steht als das gruene, endet das rote Bild frueher - genau daran
	   erkennt man ein schlecht justiertes Geraet, noch bevor man auf eine
	   Kante im Bild schaut.

	   Das Rechteck ist um edge groesser als das Bild. Damit liegt die Kante
	   ausserhalb der gezeichneten Flaeche, und ohne Woelbung bleibt innen
	   alles unangetastet: das Bild ist dann Punkt fuer Punkt so gross wie bei
	   jedem anderen Filter. Frueher fiel die aeusserste Spalte auf die halbe
	   Helligkeit, ohne dass irgendetwas daran richtig gewesen waere. */
	"float rasterMask(vec2 wc, float r, float edge)\n"
	"{\n"
	"    vec2  q  = abs(wc) - (1.0 - r + edge);\n"
	"    float sd = length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - r;\n"
	"    return 1.0 - smoothstep(-edge, edge, sd);\n"
	"}\n"

	"vec3 fetchRaw(vec2 uv)\n"
	"{\n"
	"    vec2 p = clamp(uv * FrameSize, vec2(0.5), FrameSize - 0.5);\n"
	"    return texture2D(decal, p / TextureSize).rgb;\n"
	"}\n"

	"void main()\n"
	"{\n"
	/* texCoord laeuft nur ueber den benutzten Teil der Textur; hier auf 0..1
	   ueber das Bild bringen. */
	"    vec2 uv = texCoord * TextureSize / FrameSize;\n"

	/* --- Woelbung: Ausgabepunkt -> Quellpunkt ------------------------- */
	"    vec2 p = uv * 2.0 - 1.0;\n"
	"    float a = Curvature * CURVE_X;\n"
	"    float b = Curvature * CURVE_Y;\n"
	"    vec2 w = vec2(p.x * (1.0 + a * p.y * p.y),\n"
	"                  p.y * (1.0 + b * p.x * p.x));\n"
	"    vec2 suv = w * 0.5 + 0.5;\n"

	/* --- Konvergenz, erster Teil: wie weit Rot und Blau danebenliegen -- */
	/* Steht hier oben, weil schon der Rand es braucht. cx ist der Versatz je
	   Strahl in Texturkoordinaten, cw derselbe in w - Rot liest um cx weiter
	   rechts, sein Bild liegt also um cx weiter links. */
	"    float cx = Convergence * CONVERGENCE_MAX * w.x / FrameSize.x;\n"
	"    float cw = 2.0 * cx;\n"
	"    bool  converge = (CONVERGENCE_MAX > 0.0 && Convergence > 0.0);\n"

	/* --- Rand: abgerundetes Rechteck, je Kanal ------------------------ */
	"    float r = CORNER_RADIUS * Curvature;\n"
	"    float edge = 2.0 / FrameSize.y;\n"
	"    vec3  vis = vec3(rasterMask(w, r, edge));\n"
	"    if(converge)\n"
	"    {\n"
	"        vis.r = rasterMask(vec2(w.x + cw, w.y), r, edge);\n"
	"        vis.b = rasterMask(vec2(w.x - cw, w.y), r, edge);\n"
	"    }\n"
	"    if(max(max(vis.r, vis.g), vis.b) <= 0.0)\n"
	"    { gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0); return; }\n"

	/* --- senkrecht abtasten: zwei Zeilen, nach Strahlprofil gewichtet -- */
	/* Das ist die Neuabtastung und haelt die Helligkeit; der Streifen kommt
	   danach getrennt dazu, damit der Regler wirklich bei 0 anfaengt. */
	"    float sy   = suv.y * FrameSize.y;\n"
	"    float rowc = floor(sy - 0.5) + 0.5;\n"
	"    float fy   = sy - rowc;\n"
	"    float g0   = exp(-(fy * fy) / (BEAM_WIDTH * BEAM_WIDTH));\n"
	"    float g1   = exp(-((1.0 - fy) * (1.0 - fy)) / (BEAM_WIDTH * BEAM_WIDTH));\n"
	"    float gs   = max(g0 + g1, 1e-4);\n"
	"    vec3 c0 = toLinear(fetch(vec2(suv.x, rowc / FrameSize.y)));\n"
	"    vec3 c1 = toLinear(fetch(vec2(suv.x, (rowc + 1.0) / FrameSize.y)));\n"
	"    vec3 col = (g0 * c0 + g1 * c1) / gs;\n"

	/* --- Konvergenz, zweiter Teil: Rot und Blau daneben --------------- */
	/* Gruen bleibt liegen und ist damit die Bezugsfarbe, so wie am Geraet
	   auch auf Gruen justiert wurde. Rot und Blau werden gegenlaeufig
	   verschoben, proportional zu w.x - also null in der Mitte und am Rand am
	   groessten. Der Rand des Rasters ist oben schon je Kanal gestellt; hier
	   kommt nur noch der Inhalt dazu.

	   Nur waagerecht, obwohl ein Strahl auch senkrecht danebenliegen konnte:
	   dafuer braeuchte jeder Kanal seine eigenen zwei Zeilen und ein eigenes
	   Strahlprofil, also acht Griffe statt vier, und die Zeilenstruktur der
	   Roehre verdeckt eine senkrechte Verschiebung ohnehin fast ganz. Der
	   Saum, an den sich jemand erinnert, steht an senkrechten Kanten.

	   Vier zusaetzliche Griffe - die Neuabtastung verdreifacht sich, und das
	   ist bezahlt, denn der Regler steht wie die anderen auf 0.5. Gemessen auf
	   llvmpipe bei 1280x960 kostet ein presentFrame() damit 30.6 statt 25.3 ms,
	   ein Fuenftel mehr; auf einer richtigen Grafikkarte ist es nichts. Das if
	   holt es fuer den zurueck, der den Regler ganz herunterdreht: die
	   Bedingung ist fuer den ganzen Zeichenaufruf dieselbe. */
	"    if(converge)\n"
	"    {\n"
	"        vec3 r0 = toLinear(fetch(vec2(suv.x + cx, rowc / FrameSize.y)));\n"
	"        vec3 r1 = toLinear(fetch(vec2(suv.x + cx, (rowc + 1.0) / FrameSize.y)));\n"
	"        vec3 b0 = toLinear(fetch(vec2(suv.x - cx, rowc / FrameSize.y)));\n"
	"        vec3 b1 = toLinear(fetch(vec2(suv.x - cx, (rowc + 1.0) / FrameSize.y)));\n"
	"        col.r = (g0 * r0.r + g1 * r1.r) / gs;\n"
	"        col.b = (g0 * b0.b + g1 * b1.b) / gs;\n"
	"    }\n"

	/* --- Halation ----------------------------------------------------- */
	/* Acht Griffe auf einem Ring. Der Hof ist weich, da faellt die Sternform
	   nicht auf. Er ist zugleich der teuerste Teil des Shaders - gemessen
	   knapp die Haelfte -, deshalb steht er in einem if auf eine Konstante:
	   BLOOM_STRENGTH auf 0 gesetzt, und der Uebersetzer wirft den ganzen Block
	   weg (nachgemessen: 7.9 faellt dann auf 4.2). */
	"    if(BLOOM_STRENGTH > 0.0 && Bloom > 0.0)\n"
	"    {\n"
	/* Zwei Ringe statt einem, bei gleicher Zahl von Griffen: vier waagerecht
	   und senkrecht auf dem inneren, vier diagonal auf dem aeusseren. Acht
	   Griffe auf einem einzigen Radius geben einen scharf begrenzten Ring
	   statt eines Hofes - der Uebergang war nur wenige Pixel breit. Mit zwei
	   Radien faellt es weich ueber die ganze Strecke ab. */
	"    vec2 br = BLOOM_RADIUS / FrameSize;\n"
	"    vec2 bo = br * BLOOM_OUTER;\n"
	"    vec3 t0 = fetchRaw(suv + vec2( br.x,  0.0));\n"
	"    vec3 t1 = fetchRaw(suv + vec2(-br.x,  0.0));\n"
	"    vec3 t2 = fetchRaw(suv + vec2( 0.0,  br.y));\n"
	"    vec3 t3 = fetchRaw(suv + vec2( 0.0, -br.y));\n"
	"    vec3 t4 = fetchRaw(suv + vec2( bo.x * 0.7,  bo.y * 0.7));\n"
	"    vec3 t5 = fetchRaw(suv + vec2(-bo.x * 0.7,  bo.y * 0.7));\n"
	"    vec3 t6 = fetchRaw(suv + vec2( bo.x * 0.7, -bo.y * 0.7));\n"
	"    vec3 t7 = fetchRaw(suv + vec2(-bo.x * 0.7, -bo.y * 0.7));\n"
	"    vec3 sum = t0*t0 + t1*t1 + t2*t2 + t3*t3\n"
	"             + t4*t4 + t5*t5 + t6*t6 + t7*t7;\n"
	"    vec3 halo = max(sum * 0.125 - vec3(BLOOM_THRESHOLD), vec3(0.0));\n"
	"    col += halo * BLOOM_STRENGTH * Bloom;\n"
	"    }\n"

	/* --- Flimmern, Teil 1: die Terme ---------------------------------- */
	/* Sie werden zweimal gebraucht - fuer die Helligkeit weiter unten und fuer
	   die Lage der Zeilen gleich hier -, also einmal ausrechnen. Alle
	   Frequenzen sind ganze Durchlaeufe je FLICKER_CYCLE, damit die Uhr
	   nahtlos umlaufen kann; 97, 151 und 233 je 8 s sind rund 12, 19 und
	   29 Hz, und sie sind teilerfremd, damit sich die Ueberlagerung nicht
	   schon frueher wiederholt. */
	"    float hum = 0.0;\n"
	"    float wob = 0.0;\n"
	"    if(Flicker > 0.0 || ScanFlicker > 0.0)\n"
	"    {\n"
	"        float w = 6.2831853 / FLICKER_CYCLE;\n"
	"        hum = sin((uv.y * HUM_BARS) * 6.2831853 - Time * w * HUM_ROLLS);\n"
	"        wob = sin(Time * w *  97.0) * 0.5\n"
	"            + sin(Time * w * 151.0) * 0.3\n"
	"            + sin(Time * w * 233.0) * 0.2;\n"
	"    }\n"

	/* --- Zeilenstruktur ----------------------------------------------- */
	/* Abstand zur naechsten Zeilenmitte, gemessen in Perioden. Bei Periode 1
	   liegen bei 2x beide Ausgabezeilen gleich weit weg und es ist nichts zu
	   sehen; das ist richtig so und der Grund fuer SCANLINE_PERIOD.

	   ScanPhase schiebt das ganze Muster langsam nach unten, CRAWL_JITTER
	   laesst es dabei zittern. Ist Scanline 0, faellt beides von selbst weg -
	   dann gibt es keine Zeilen, die kriechen koennten. */
	"    float ph = sy / SCANLINE_PERIOD + ScanPhase + ScanFlicker * CRAWL_JITTER * wob;\n"
	"    float dc = abs(fract(ph) - 0.5) * 2.0;\n"
	"    float k  = BEAM_WIDTH * BEAM_WIDTH * 4.0;\n"
	"    float beam = exp(-(dc * dc) / k);\n"
	/* Der Mittelwert des Profils ueber eine Periode, mit fuenf Stuetzstellen
	   genaehert. Damit hat beam/SCAN_AVG den Mittelwert 1, und mix(1, ., S)
	   behaelt ihn fuer jede Reglerstellung - die Streifen kosten also keine
	   Helligkeit mehr, egal wie schmal der Strahl eingestellt ist. Alle
	   Eingaben sind Konstanten, das faltet der Uebersetzer weg. */
	"    float scanAvg = (exp(-0.01 / k) + exp(-0.09 / k) + exp(-0.25 / k)\n"
	"                   + exp(-0.49 / k) + exp(-0.81 / k)) * 0.2;\n"
	"    col *= mix(1.0, beam / max(scanAvg, 1e-3), Scanline);\n"

	/* --- Maske, im Ausgabepixelraster --------------------------------- */
	/* gl_FragCoord ist in Fensterpixeln - genau richtig, denn eine Lochmaske
	   sitzt auf dem Glas und nicht im Signal. */
	"    float mp = floor(mod(gl_FragCoord.x, MASK_PITCH));\n"
	"    vec3 mask = vec3(1.0 - MASK_STRENGTH);\n"
	"    if(mp < 0.5)      mask.r = 1.0;\n"
	"    else if(mp < 1.5) mask.g = 1.0;\n"
	"    else              mask.b = 1.0;\n"
	/* Je Kanal ist ein Streifen von MASK_PITCH hell und der Rest gedimmt; das
	   ist der Mittelwert davon. Geteilt wird dadurch, damit die Maske genauso
	   viel Licht durchlaesst wie gar keine Maske. */
	"    float maskAvg = (1.0 + (MASK_PITCH - 1.0) * (1.0 - MASK_STRENGTH)) / MASK_PITCH;\n"
	"    col *= mask / maskAvg;\n"

	/* --- Licht zurueckgeben, Rand, Gamma ------------------------------ */
	/* --- Flimmern, Teil 2: die Helligkeit ------------------------------ */
	/* Beide Anteile schwingen um null, die mittlere Helligkeit bleibt also
	   stehen. */
	"    col *= 1.0 + Flicker * (HUM_DEPTH * hum + FLICKER_DEPTH * wob);\n"

	"    col *= BRIGHTNESS;\n"
	"    float vig = 1.0 - VIGNETTE * dot(w, w) * 0.5;\n"
	"    col *= max(vig, 0.0) * vis;\n"
	"    gl_FragColor = vec4(pow(max(col, vec3(0.0)), vec3(1.0 / GAMMA_OUT)), 1.0);\n"
	"}\n";

U_Crt::U_Crt()
{
	locScanline = -1;
	locCurvature = -1;
	locBloom = -1;
	locFlicker = -1;
	locScanFlicker = -1;
	locConvergence = -1;
	locTime = -1;
	locScanPhase = -1;
	scanline = 0.5;
	curvature = 0.5;
	bloom = 0.5;
	flicker = 0.5;
	scanFlicker = 0.5;
	convergence = 0.5;
}

U_Crt::~U_Crt()
{
}

bool U_Crt::createGL()
{
	if(!program.create(p_crtFragmentShader, "crt fragment")) return false;

	locScanline    = glExtGetUniformLocation(program.id, "Scanline");
	locCurvature   = glExtGetUniformLocation(program.id, "Curvature");
	locBloom       = glExtGetUniformLocation(program.id, "Bloom");
	locFlicker     = glExtGetUniformLocation(program.id, "Flicker");
	locScanFlicker = glExtGetUniformLocation(program.id, "ScanFlicker");
	locConvergence = glExtGetUniformLocation(program.id, "Convergence");
	locTime        = glExtGetUniformLocation(program.id, "Time");
	locScanPhase   = glExtGetUniformLocation(program.id, "ScanPhase");
	return true;
}

void U_Crt::destroyGL()
{
	// Die Uniformstellen bleiben stehen. Sie sind ohnehin nur gueltig, solange
	// es ein Programm gibt, und eine zweite Liste waere eine zweite Liste, die
	// jemand zu ergaenzen vergisst.
	program.destroy();
}

void U_Crt::present(const PresentContext& context)
{
	program.use(context);

	PresentProgram::setUniform(locScanline, scanline);
	PresentProgram::setUniform(locCurvature, curvature);
	PresentProgram::setUniform(locBloom, bloom);
	PresentProgram::setUniform(locFlicker, flicker);
	PresentProgram::setUniform(locScanFlicker, scanFlicker);
	PresentProgram::setUniform(locConvergence, convergence);

	// Die Wanduhr, nicht Engine::getTime() - die zaehlt in Logikschritten und
	// steht bei Pause still; ein Bildschirm flimmert auch dann. Der Umlauf ist
	// CRT_FLICKER_CYCLE, alle Frequenzen darin sind ganze Vielfache davon, also
	// springt beim Umlauf nichts.
	const double seconds = static_cast<double>(SDL_GetTicks()) * 0.001;
	PresentProgram::setUniform(locTime, fmod(seconds, CRT_FLICKER_CYCLE));

	// Das Zeilenkriechen wird als einziger Anteil hier gerechnet: es ist eine
	// Rampe, keine Schwingung, und ihre Steigung haengt am Regler - aus der
	// schon gekuerzten Uhr spraenge die Phase bei jedem Umlauf.
	PresentProgram::setUniform(locScanPhase,
							   fmod(seconds * crtCrawlSpeed * scanFlicker, 1.0));

	program.drawQuad(context);
}

Vec2d U_Crt::warpToSource(const Vec2d& p) const
{
	// Genau die Formel aus dem Shader oben. p und der Rueckgabewert laufen von
	// -1 bis 1 ab der Bildmitte.
	if(curvature <= 0.0) return p;

	const double a = curvature * crtCurveX;
	const double b = curvature * crtCurveY;
	return Vec2d(p.x * (1.0 + a * p.y * p.y),
				 p.y * (1.0 + b * p.x * p.x));
}

Vec2d U_Crt::warpToOutput(const Vec2d& s) const
{
	if(curvature <= 0.0) return s;

	const double a = curvature * crtCurveX;
	const double b = curvature * crtCurveY;

	// Die Umkehrung. Das Gleichungspaar ist gekoppelt - x haengt an y und
	// umgekehrt - und hat keine geschlossene Loesung; als Fixpunkt
	//
	//     x <- u / (1 + a*y^2)      y <- v / (1 + b*x^2)
	//
	// zieht es sich sehr schnell zusammen: nach acht Runden liegt der Fehler
	// selbst bei uebertriebener Woelbung unter 2.3e-4 Bildpunkten.
	double x = s.x;
	double y = s.y;
	for(int i = 0; i < 8; i++)
	{
		x = s.x / (1.0 + a * y * y);
		y = s.y / (1.0 + b * x * x);
	}
	return Vec2d(x, y);
}

void U_Crt::setScanline(double value)    { scanline = clamp(value, 0.0, 1.0); }
void U_Crt::setCurvature(double value)   { curvature = clamp(value, 0.0, 1.0); }
void U_Crt::setBloom(double value)       { bloom = clamp(value, 0.0, 1.0); }
void U_Crt::setFlicker(double value)     { flicker = clamp(value, 0.0, 1.0); }
void U_Crt::setScanFlicker(double value) { scanFlicker = clamp(value, 0.0, 1.0); }
void U_Crt::setConvergence(double value) { convergence = clamp(value, 0.0, 1.0); }

void U_Crt::loadConfig(TiXmlElement* p_config)
{
	TiXmlElement* p_crt = p_config->FirstChildElement("CrtUpscaler");
	if(!p_crt) return;

	// Nur lesen, was dasteht, und nichts zurueckstellen: der Abbrechen-Knopf
	// des Optionsdialogs ruft loadConfig() mitten im Spiel, und beim ersten
	// Start gibt es die Datei noch gar nicht.
	double value = 0.0;
	if(p_crt->QueryDoubleAttribute("scanline", &value) == TIXML_SUCCESS)    setScanline(value);
	if(p_crt->QueryDoubleAttribute("curvature", &value) == TIXML_SUCCESS)   setCurvature(value);
	if(p_crt->QueryDoubleAttribute("bloom", &value) == TIXML_SUCCESS)       setBloom(value);
	if(p_crt->QueryDoubleAttribute("flicker", &value) == TIXML_SUCCESS)     setFlicker(value);
	if(p_crt->QueryDoubleAttribute("scanFlicker", &value) == TIXML_SUCCESS) setScanFlicker(value);
	if(p_crt->QueryDoubleAttribute("convergence", &value) == TIXML_SUCCESS) setConvergence(value);
}

void U_Crt::saveConfig(TiXmlElement* p_config)
{
	TiXmlElement* p_crt = new TiXmlElement("CrtUpscaler");
	p_crt->SetDoubleAttribute("scanline", scanline);
	p_crt->SetDoubleAttribute("curvature", curvature);
	p_crt->SetDoubleAttribute("bloom", bloom);
	p_crt->SetDoubleAttribute("flicker", flicker);
	p_crt->SetDoubleAttribute("scanFlicker", scanFlicker);
	p_crt->SetDoubleAttribute("convergence", convergence);
	p_config->LinkEndChild(p_crt);
}
