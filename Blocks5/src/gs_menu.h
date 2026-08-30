#ifndef _GS_MENU_H
#define _GS_MENU_H

/*** Klasse fuer das Menue ***/

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
	// Import und Export. Der Dateidialog des Browsers meldet sich asynchron,
	// der von Windows modal - pollImport() verdeckt beides.
	void pollImport();
	void pollExport();
	void showMessage(const std::string& text);
	void refreshExportList();
	int currentExportKind() const;

	Engine& engine;
	Texture* p_clouds;
	Texture* p_background;
	Level* p_titleLevel;
	TiXmlDocument titleLevelXML;
	bool levelSaved;
	Options* p_options;
	Help* p_help;
	uint time;

	// Der Export wartet genau wie der Import eine Runde: unter Windows startet
	// der Dateidialog eine zweite Nachrichtenschleife, und die darf nicht
	// mitten in GUI_Button::onMouseUp anfangen.
	int pendingExportKind;
	std::string pendingExportName;
	bool pendingExport;
	std::unordered_map<uint, std::list<uint> > keyData;
};

#endif