#ifndef _GS_GAME_H
#define _GS_GAME_H

/*** Class for the main game state ***/

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

	// The level being played, for the test hook's particle dump; 0 outside
	// a level.
	Level* getLevel() const { return p_level; }

	void onRender();
	void onUpdate();
	bool getMouseDragCells(Vec2i* p_actor, Vec2i* p_target);
	bool canMouseDragStep(const Vec2i& dir);
	void onEnter(const ParameterBlock& context);
	void onLeave(const ParameterBlock& context);
	void onGetFocus();
	void onLoseFocus();
	void onAppLoseFocus();

	// Is the game paused? The test hook reports it - from outside the pause is
	// otherwise only recognisable by a piece of lettering that changes colour.
	bool isPaused() const { return paused; }

private:
	int loadLevel();

	// Work whatever the active character is standing next to, the way a walk
	// into it would. GameGUI's press is the only caller.
	void bumpCell(const Vec2i& cell);

	// The file the running level came from. Only the single levels show it;
	// remembered rather than looked up, because levelNumber already stands one
	// further on after the last diamond.
	std::string levelFilename;

	Engine& engine;
	Level* p_level;
	GS_SelectLevel* p_selectLevel;
	Texture* p_misc;
	int leaveCountDown;
	bool cameFromEditor;
	TiXmlDocument* p_originalLevel;
	TiXmlDocument* p_saveGame;
	uint levelNumber;
	Campaign* p_currentCampaign;
	uint showCursor;
	bool ignoreNextCursorMovement;

	// Did the press that is being held land on a character? A drag is a
	// command to the one the player grabbed, so it has to start on it -
	// dragging from empty ground steers nobody. Set by the press that
	// found one, which is the same press that wakes it up, and cleared
	// when the last button comes up rather than on the next press,
	// because the second button of a two-button grip lands wherever the
	// cursor has got to by then.
	bool dragFromPlayer;

	bool paused;
	Vec2f pausePosition;
	Vec2f pauseVelocity;
};

#endif