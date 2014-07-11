#ifndef _GS_GAME_H
#define _GS_GAME_H

/*** Klasse für den Hauptspielzustand ***/

#include "gamestate.h"
#include "engine.h"

class TileSet;
class Texture;
class Level;
class Campaign;
class GS_SelectLevel;

class GS_Game : public GameState
{
	friend class GameGUI;

public:
	GS_Game();
	~GS_Game();

	void onRender();
	void onUpdate();
	void onEnter(const ParameterBlock& context);
	void onLeave(const ParameterBlock& context);
	void onGetFocus();
	void onLoseFocus();
	void onAppLoseFocus();

private:
	void updateMusic();
	int loadLevel();

	Engine& engine;
	Level* p_level;
	GS_SelectLevel* p_selectLevel;
	Texture* p_misc;
	int leaveCountDown;
	int switchTimer;
	bool cameFromEditor;
	TiXmlDocument* p_originalLevel;
	TiXmlDocument* p_saveGame;
	uint levelNumber;
	Campaign* p_currentCampaign;
	uint showCursor;
	bool ignoreNextCursorMovement;
	bool paused;
	Vec2d pausePosition;
	Vec2d pauseVelocity;
};

#endif