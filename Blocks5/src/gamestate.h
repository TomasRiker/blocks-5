#ifndef _GAMESTATE_H
#define _GAMESTATE_H

/*** Class for a game state ***/

#include "parameterblock.h"
#include "gui.h"

class GameState : public sigslot::has_slots<>
{
public:
	GameState(const std::string& name);
	virtual ~GameState();

	virtual void onEnter(const ParameterBlock& context);
	virtual void onLeave(const ParameterBlock& context);
	virtual void onGetFocus();
	virtual void onLoseFocus();
	virtual void onRender();
	virtual void onUpdate();
	virtual void onAppGetFocus();
	virtual void onAppLoseFocus();

	// Where a mouse drag would steer: the cell the active character stands on
	// and the cell under the cursor. Only GS_Game has either, and only while a
	// level is running with somebody awake - everything else says no and the
	// drag stands still. Engine::updateMouseDrag asks, from updateVKs() and
	// not from onUpdate(), which runs after updateActions(): a step decided
	// there would reach the character a tick late at both ends, and the late
	// one at the end is a step taken after the player let go.
	virtual bool getMouseDragCells(Vec2i* p_actor, Vec2i* p_target);

	const std::string& getName() const;

protected:
	GUI& gui;

private:
	std::string name;
};

#endif