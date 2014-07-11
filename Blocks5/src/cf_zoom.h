#ifndef _CF_ZOOM_H
#define _CF_ZOOM_H

#include "crossfade.h"

/*** Überblendung durch Zoom ***/

class CF_Zoom : public Crossfade
{
public:
	CF_Zoom(const Vec2i& targetIn, const Vec2i& targetOut);
	~CF_Zoom();

	void render(double t, uint oldImageID, uint newImageID);

private:
	const Vec2i targetIn;
	const Vec2i targetOut;
};

#endif