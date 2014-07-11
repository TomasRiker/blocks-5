#ifndef _GS_MENU_H
#define _GS_MENU_H

/*** Klasse für das Menü ***/

#include "gamestate.h"
#include "engine.h"
#include "level.h"

class GUI_Element;
class Texture;
class Options;
class Help;

class GS_Menu : public GameState
{
public:
	GS_Menu();
	~GS_Menu();

	void onRender();
	void onUpdate();
	void onEnter(const ParameterBlock& context);
	void onLeave(const ParameterBlock& context);
	void onGetFocus();
	void onLoseFocus();

	void handleClick(GUI_Element* p_element);

private:
	Engine& engine;
	Texture* p_clouds;
	Texture* p_background;
	Level* p_titleLevel;
	TiXmlDocument titleLevelXML;
	bool levelSaved;
	Options* p_options;
	Help* p_help;
	uint time;
	stdext::hash_map<uint, std::list<uint> > keyData;
};

#endif