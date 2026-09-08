#ifndef _GS_SELECTLEVEL_H
#define _GS_SELECTLEVEL_H

/*** Class for the level selection ***/

#include "gamestate.h"
#include "engine.h"
#include "level.h"
#include "progressdb.h"

class GUI_Element;
class Texture;
class Campaign;
class Level;

class GS_SelectLevel : public GameState
{
public:
	GS_SelectLevel();
	~GS_SelectLevel();

	void onRender();
	void onUpdate();
	void onEnter(const ParameterBlock& context);
	void onLeave(const ParameterBlock& context);
	void onGetFocus();
	void onLoseFocus();

	void handleClick(GUI_Element* p_element);
	void setCurrentLevel(uint currentLevel);

private:
	Engine& engine;
	Texture* p_background;
	Texture* p_misc;
	std::vector<Campaign*> campaigns;
	Campaign* p_currentCampaign;
	Level* p_currentLevel;
	uint currentLevel;

	// What the progress database said when the screen last got the focus. The
	// database itself keeps nothing, and the two readers below run inside
	// onRender(), so asking it per frame would mean opening the archive per
	// frame. Re-read in onGetFocus(), which is where a played level comes back
	// to - the screen therefore never shows a level it just saw solved as
	// unsolved.
	ProgressDB::Progress progress;

	void refreshProgress();
	uint getNumLevelsCompleted() const;
	bool wasLevelCompleted(uint level) const;

	void loadLevel();
	int getLevelStatus(uint level);
	void updateNote();
	void pressButton(GUI_Element* p_button);
	void selectCampaign(int delta);
};

#endif