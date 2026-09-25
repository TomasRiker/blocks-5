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

	// Where a mouse drag would steer: the active character's cell and the cell
	// under the cursor. Only GS_Game answers, while a level runs with somebody
	// awake; elsewhere the drag stands still. Engine::updateMouseDrag asks from
	// updateVKs(), before updateActions(): from onUpdate() every step would
	// arrive a tick late, and the last one after the player had let go.
	virtual bool getMouseDragCells(Vec2i* p_actor, Vec2i* p_target);

	// Whether the dragged character can take one step this way. A blocked
	// direction is never commanded, or the key would hold the character
	// against the wall while the button is down, and the other axis, which
	// could walk around, would never get its turn. Asked only of a state that
	// answered getMouseDragCells.
	virtual bool canMouseDragStep(const Vec2i& dir);

	const std::string& getName() const;

protected:
	GUI& gui;

private:
	std::string name;
};

#endif