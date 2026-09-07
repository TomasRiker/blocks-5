#ifndef _GS_MENU_H
#define _GS_MENU_H

/*** Class for the menu ***/

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
	// The Manager: import, export, delete. The browser's file dialog answers
	// asynchronously, the Windows one is modal - pollImport() hides the
	// difference.
	void pollImport();
	void pollExport();
	void openManager();
	void refreshManagerList();
	void updateManagerButtons();
	int currentManagerKind() const;
	void setManagerKind(int kind);

	Engine& engine;
	Texture* p_clouds;
	Texture* p_background;
	Level* p_titleLevel;
	TiXmlDocument titleLevelXML;
	bool levelSaved;
	Options* p_options;
	Help* p_help;
	uint time;

	// The export waits one round just as the import does: under Windows the
	// file dialog starts a second message loop, and that must not begin in
	// the middle of GUI_Button::onMouseUp.
	int pendingExportKind;
	std::string pendingExportName;
	bool pendingExport;

	// What the confirmation is to delete, recorded on the click on Delete.
	int pendingDeleteKind;
	std::string pendingDeleteName;
	std::unordered_map<uint, std::list<uint> > keyData;
};

#endif