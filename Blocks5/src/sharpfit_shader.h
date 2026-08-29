#ifndef _SHARPFIT_SHADER_H
#define _SHARPFIT_SHADER_H

/* "Scharf, angepasst" - nearest-Optik bei krummem Vergroesserungsfaktor.

   Gedacht ist es so: das 640x480-Bild erst mit nearest um den kleinsten
   ganzzahligen Faktor N vergroessern, mit dem es mindestens so gross wie das
   Zielrechteck wird, und das Ergebnis dann auf die tatsaechliche Groesse
   herunterrechnen. Nearest allein braucht ein ganzzahliges Verhaeltnis, sonst
   verdoppelt es manche Quellpixel und andere nicht; hier faellt der krumme Rest
   in den zweiten Schritt, wo er nur noch eine weiche Kante von etwa einem Pixel
   erzeugt statt ungleicher Strichstaerken.

   Zwei Durchgaenge braucht das nicht. Bilinear ueber ein nearest-vergroessertes
   Bild ist stueckweise linear: innerhalb eines Quellpixels konstant, und an
   jeder Pixelgrenze eine Rampe von genau 1/N Quellpixeln Breite. Genau das
   liefert auch die Hardware, wenn man die Originaltextur bilinear abtastet und
   die Texturkoordinate vorher durch dieselbe stueckweise lineare Funktion
   schickt - ein Fetch statt zweier Durchgaenge, und exakt dasselbe Ergebnis.

   Die Umrechnung, eindimensional, mit s = Position im Quellpixel (0..1):

       d    = s - 0.5                       Abstand von der Pixelmitte
       flat = 0.5 - 0.5 / N                 halbe Breite des flachen Teils
       f    = (d - clamp(d, -flat, flat)) * N + 0.5

   Fuer |d| <= flat ist f = 0.5, also genau die Pixelmitte: die Hardware liefert
   das Texel unveraendert, so scharf wie nearest. Ausserhalb laeuft f linear
   nach 0 beziehungsweise 1, was der Rampe an der Pixelgrenze entspricht. Bei
   N = 1 verschwindet der flache Teil und es bleibt gewoehnliches Bilinear.

   Dieselbe Rechnung ist in Emulatorkreisen als "sharp bilinear" bekannt
   (Themaister, libretro); hergeleitet ist sie hier neu, Code ist keiner
   uebernommen.

   Die Textur MUSS bilinear abgetastet werden - der ganze Trick besteht darin,
   die Hardware-Interpolation zu benutzen. Mit GL_NEAREST kaeme wieder nur
   nearest heraus.

   Kein #version: 110 auf dem Desktop, 100 unter GLSL ES, und der Quelltext
   uebersetzt als beides. */

/* Die Eckpunkte kommen fertig in Clipkoordinaten aus presentFrame(), es gibt
   also nichts zu transformieren. Kein Anfassen des Fixed-Function-Zustands, und
   im Browser damit auch keine Beruehrung mit Emscriptens
   Immediate-Mode-Nachbau. */
static const char* p_presentVertexShader =
	"#ifdef GL_ES\n"
	"precision highp float;\n"
	"#endif\n"
	"attribute vec2 aPosition;\n"
	"attribute vec2 aTexCoord;\n"
	"varying vec2 texCoord;\n"
	"void main()\n"
	"{\n"
	"    texCoord = aTexCoord;\n"
	"    gl_Position = vec4(aPosition, 0.0, 1.0);\n"
	"}\n";

static const char* p_sharpFitFragmentShader =
	"#ifdef GL_ES\n"
	"#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
	"precision highp float;\n"
	"#else\n"
	"precision mediump float;\n"
	"#endif\n"
	"#endif\n"
	"uniform sampler2D decal;\n"
	"uniform vec2 TextureSize;\n"   /* ganze Textur, Zweierpotenz */
	"uniform vec2 FrameSize;\n"     /* der benutzte Teil davon, 640x480 */
	"uniform vec2 Prescale;\n"      /* N, ganzzahlig, >= 1 */
	"varying vec2 texCoord;\n"
	"void main()\n"
	"{\n"
	"    vec2 texel = texCoord * TextureSize;\n"
	"    vec2 base  = floor(texel);\n"
	"    vec2 d     = (texel - base) - 0.5;\n"
	"    vec2 flat_ = 0.5 - 0.5 / Prescale;\n"
	"    vec2 f     = (d - clamp(d, -flat_, flat_)) * Prescale + 0.5;\n"
	/* Am rechten und oberen Rand darf die Rampe nicht in den ungenutzten Teil
	   der Zweierpotenz-Textur hineingreifen. */
	"    vec2 p     = clamp(base + f, vec2(0.5), FrameSize - 0.5);\n"
	"    gl_FragColor = vec4(texture2D(decal, p / TextureSize).rgb, 1.0);\n"
	"}\n";

#endif
