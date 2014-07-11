#ifndef _GS_LOADING_H
#define _GS_LOADING_H

/*** Klasse für den Ladebildschirm ***/

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
};

#endif