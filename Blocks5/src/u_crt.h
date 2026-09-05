#ifndef _U_CRT_H
#define _U_CRT_H

#include "upscaler.h"

/*** "Roehrenmonitor" - ein Bildschirm aus den neunziger Jahren ***/

// Was den Filter ausmacht, steht als Konstante ueber dem Shader in u_crt.cpp
// und ist zum Verstellen gedacht; die sechs Regler hier sind die, die eine
// Frage des Geschmacks und nicht der Abstimmung sind.
//
// Die Woelbung geht auch durch die Maus: warpToSource() ist die Formel des
// Shaders noch einmal in C++, warpToOutput() ihre Umkehrung, und
// Engine::getCursorPosition() haengt daran.
class U_Crt : public Upscaler
{
public:
	U_Crt();
	~U_Crt();

	const char* getName() const { return "Crt"; }
	// Sharp-fit rechnet die Texturkoordinate so um, dass die
	// Hardware-Interpolation das nearest-Ergebnis liefert; die Roehre tut
	// dasselbe, nur weicher. Beide brauchen dafuer GL_LINEAR.
	GLint getTextureFilter() const { return GL_LINEAR; }

	bool createGL();
	void destroyGL();
	bool isAvailable() const { return program.isLinked(); }

	void present(const PresentContext& context);

	Vec2d warpToSource(const Vec2d& p) const;
	Vec2d warpToOutput(const Vec2d& s) const;

	// Um so viel steht das Raster vom Rand des Glases ab, in Anteilen der
	// halben Bildbreite - damit die weiche Randkante und die Farbsaeume auch
	// an den Kantenmitten noch Platz haben. Bei Woelbung 0 ist es 0, und das
	// Bild deckt sich dann genau mit dem der anderen Filter.
	double getOverscan() const;
	bool distortsCursor() const { return curvature > 0.0; }

	void loadConfig(TiXmlElement* p_config);
	void saveConfig(TiXmlElement* p_config);

	// Die Regler, je 0..1; sie wirken sofort, ohne den Shader neu zu
	// uebersetzen. 0 heisst bei jedem "Effekt aus".
	double getScanline() const { return scanline; }
	double getCurvature() const { return curvature; }
	double getBloom() const { return bloom; }
	double getFlicker() const { return flicker; }
	double getScanFlicker() const { return scanFlicker; }
	double getConvergence() const { return convergence; }
	void setScanline(double value);
	void setCurvature(double value);
	void setBloom(double value);
	void setFlicker(double value);
	void setScanFlicker(double value);
	void setConvergence(double value);

private:
	PresentProgram program;

	// Die Uniformstellen, die es nur hier gibt. Je eine eigene Zeile, damit
	// Tools/verify.py sie sieht - eine Sammeldeklaration uebersieht es, und
	// genau daran hing "convergence".
	int locScanline;
	int locCurvature;
	int locBloom;
	int locFlicker;
	int locScanFlicker;
	int locConvergence;
	int locOverscan;
	int locTime;
	int locScanPhase;

	// Die Bildgroesse, wie sie zuletzt im PresentContext stand; getOverscan()
	// rechnet Quellzeilen und -spalten damit in Anteile des Bildes um.
	Vec2i frameSize;

	double scanline;
	double curvature;
	double bloom;
	double flicker;
	double scanFlicker;
	double convergence;
};

#endif
