#ifndef _GAMESTATE_H
#define _GAMESTATE_H

/*** Klasse für einen Spielzustand ***/

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

	const std::string& getName() const;

protected:
	GUI& gui;

private:
	std::string name;
};

#endif