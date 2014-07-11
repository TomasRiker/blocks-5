#ifndef _CF_CAMERA_H
#define _CF_CAMERA_H

#include "crossfade.h"

/*** Überblendung durch Kamerafahrt ***/

class CF_Camera : public Crossfade
{
public:
	CF_Camera();
	~CF_Camera();

	void render(double t, uint oldImageID, uint newImageID);
};

#endif