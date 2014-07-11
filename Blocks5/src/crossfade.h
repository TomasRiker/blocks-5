#ifndef _CROSSFADE_H
#define _CROSSFADE_H

// Klasse für Überblendungen

class Crossfade
{
public:
	Crossfade();
	virtual ~Crossfade();

	virtual void render(double t, uint oldImageID, uint newImageID);

protected:
	void setupTexCoords();

	Vec2i screenSize;
	Vec2i screenPow2Size;
};

#endif