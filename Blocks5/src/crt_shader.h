#ifndef _CRT_SHADER_H
#define _CRT_SHADER_H

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
#define CRT_STR2(x) #x
#define CRT_STR(x) CRT_STR2(x)

static const double crtCurveX = CRT_CURVE_X;
static const double crtCurveY = CRT_CURVE_Y;

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
	   Weichzeichnung. Radius in Quellpixeln. */
	"const float BLOOM_STRENGTH  = 0.28;\n"
	"const float BLOOM_THRESHOLD = 0.55;\n"
	"const float BLOOM_RADIUS    = 2.2;\n"

	/* Waagerechte Bandbreite. Das Videosignal war analog und begrenzt, deshalb
	   war eine Roehre quer weicher als senkrecht. In Bruchteilen eines
	   Quellpixels; 0 = so scharf wie "Scharf, angepasst". */
	"const float SOFTNESS = 0.35;\n"

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

	/* Der Hof braucht die Umrechnung nicht texelweise: er wird ohnehin
	   verschmiert, also reicht es, die Summe einmal ins lineare Licht zu holen
	   statt jeden Griff einzeln. Und die scharfe Umrechnung aus fetch() braucht
	   er auch nicht - ein weiches Bild von einem weichen Bild. Das spart pro
	   Ausgabepixel 24 pow() und acht Mal die Rampenrechnung; gemessen faellt der
	   Filter dadurch von 11.0 auf 3.4 mal die Kosten einer einfachen Ausgabe. */
	"vec3 toLinear(vec3 c) { return pow(max(c, vec3(0.0)), vec3(GAMMA_IN)); }\n"

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

	/* --- Rand: abgerundetes Rechteck, eine Pixelbreite weich ---------- */
	"    float r = CORNER_RADIUS * Curvature;\n"
	"    vec2  q = abs(w) - (1.0 - r);\n"
	"    float sd = length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - r;\n"
	"    float edge = 2.0 / FrameSize.y;\n"
	"    float vis = 1.0 - smoothstep(-edge, edge, sd);\n"
	"    if(vis <= 0.0) { gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0); return; }\n"

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

	/* --- Halation ----------------------------------------------------- */
	/* Acht Abtastungen auf einem Ring. Nicht mehr - der Hof ist weich, da
	   faellt die Sternform nicht auf, und jeder Griff kostet. */
	/* Acht Griffe auf einem Ring. Der Hof ist weich, da faellt die Sternform
	   nicht auf. Er ist zugleich der teuerste Teil des Shaders - gemessen
	   knapp die Haelfte -, deshalb steht er in einem if auf eine Konstante:
	   BLOOM_STRENGTH auf 0 gesetzt, und der Uebersetzer wirft den ganzen Block
	   weg (nachgemessen: 7.9 faellt dann auf 4.2). */
	"    if(BLOOM_STRENGTH > 0.0)\n"
	"    {\n"
	"    vec2 br = BLOOM_RADIUS / FrameSize;\n"
	"    vec3 sum = fetchRaw(suv + vec2( br.x,  0.0))\n"
	"             + fetchRaw(suv + vec2(-br.x,  0.0))\n"
	"             + fetchRaw(suv + vec2( 0.0,  br.y))\n"
	"             + fetchRaw(suv + vec2( 0.0, -br.y))\n"
	"             + fetchRaw(suv + vec2( br.x * 0.7,  br.y * 0.7))\n"
	"             + fetchRaw(suv + vec2(-br.x * 0.7,  br.y * 0.7))\n"
	"             + fetchRaw(suv + vec2( br.x * 0.7, -br.y * 0.7))\n"
	"             + fetchRaw(suv + vec2(-br.x * 0.7, -br.y * 0.7));\n"
	"    vec3 halo = max(toLinear(sum * 0.125) - vec3(BLOOM_THRESHOLD), vec3(0.0));\n"
	"    col += halo * BLOOM_STRENGTH;\n"
	"    }\n"

	/* --- Zeilenstruktur ----------------------------------------------- */
	/* Abstand zur naechsten Zeilenmitte, gemessen in Perioden. Bei Periode 1
	   liegen bei 2x beide Ausgabezeilen gleich weit weg und es ist nichts zu
	   sehen; das ist richtig so und der Grund fuer SCANLINE_PERIOD. */
	"    float ph = sy / SCANLINE_PERIOD;\n"
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
	"    col *= BRIGHTNESS;\n"
	"    float vig = 1.0 - VIGNETTE * dot(w, w) * 0.5;\n"
	"    col *= max(vig, 0.0) * vis;\n"
	"    gl_FragColor = vec4(pow(max(col, vec3(0.0)), vec3(1.0 / GAMMA_OUT)), 1.0);\n"
	"}\n";

#endif
