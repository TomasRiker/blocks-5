#ifndef _GS_LOADING_H
#define _GS_LOADING_H

/*** Class for the loading screen ***/

#include "gamestate.h"
#include "engine.h"

class Font;
class Texture;

class GS_Loading : public GameState
{
public:
	GS_Loading();
	~GS_Loading();

	void onRender();
	void onUpdate();
	void onEnter(const ParameterBlock& context);
	void onLeave(const ParameterBlock& context);
	void onGetFocus();
	void onLoseFocus();

private:
	void loadGraphics();
	void loadSounds();

	Engine& engine;
	Font* p_font;
	Texture* p_logo;
	int time;
	int load;
	bool soundPlayed;
	double logoSize;
	double logoSizeVel;
#ifdef __EMSCRIPTEN__
	bool waitingForClick;   // the browser blocks audio until the first gesture
	int waitTime;           // wait time in ms, drives the pulsing too
	int gestureTime;        // waitTime of the first input, otherwise -1
#endif
};

#endif