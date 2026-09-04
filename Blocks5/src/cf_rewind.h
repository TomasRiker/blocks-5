#ifndef _CF_REWIND_H
#define _CF_REWIND_H

#include "crossfade.h"

/*** Ueberblendung: das Band wird zurueckgespult ***/

class Font;

// Der Neustart eines Levels sieht aus wie ein Videorekorder im Ruecklauf. Warum
// gerade das den Sprung verdeckt, steht ueber render() in cf_rewind.cpp.
class CF_Rewind : public Crossfade
{
public:
	CF_Rewind();
	~CF_Rewind();

	void render(double t, uint oldImageID, uint newImageID);

private:
	// Ein Streifen Bild, quer ueber den Schirm: Zeile y auf dem Schirm zeigt
	// Zeile sourceY der Vorlage, um shift Pixel seitlich verrutscht.
	void drawStrip(int y, int height, int sourceY, double shift) const;

	// Ein Streifen Schnee. Die Stelle im Rauschbild wuerfelt jeder Aufruf neu.
	void drawSnow(int y, int height, double alpha) const;

	uint noiseID;
	Font* p_font;
};

#endif
