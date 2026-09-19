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

	// Whether the character a drag is steering can take one step this way. A
	// direction it cannot go must not be commanded at all: the key would hold
	// the character against the wall for as long as the button is, and since a
	// leg ends when it runs out, the axis that could still walk around the
	// obstacle would never get its turn. Asked only of a state that answered
	// getMouseDragCells.
	virtual bool canMouseDragStep(const Vec2i& dir);

	const std::string& getName() const;

protected:
	GUI& gui;

private:
	std::string name;
};

#endif