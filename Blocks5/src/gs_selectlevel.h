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

	// What the progress database said when the screen last got the focus.
	// The database keeps nothing in memory and the two readers below run in
	// onRender(), so asking it directly would open the archive every frame.
	// Re-read in onGetFocus(), where a played level returns to, so a level
	// just solved never shows as unsolved.
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