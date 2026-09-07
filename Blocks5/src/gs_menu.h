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

	// The import once the player has agreed to it, and the other answer to
	// the same question: take the imported progress into the one already
	// there instead of over it.
	void completeImport(int kind, const std::string& path, const std::string& untrustedName);
	void mergeImport(const std::string& path);

	// Put the question and take the window away again. offerMerge shows the
	// third button, which nothing but an imported progress database offers.
	void askConfirmation(const std::string& text, const std::string& yesTitle,
						 const std::string& noTitle, bool offerMerge);
	void closeConfirmation();
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

	// What the confirmation window is asking. It serves two questions now,
	// and Yes means something different for each.
	enum ConfirmMode
	{
		CONFIRM_NONE = 0,
		CONFIRM_DELETE,
		CONFIRM_OVERWRITE
	};
	int confirmMode;

	// CONFIRM_DELETE: what is to be deleted, recorded on the click on Delete.
	int pendingDeleteKind;
	std::string pendingDeleteName;

	// CONFIRM_OVERWRITE: the import, held back until the question is
	// answered. Transfer::finishImport() has to wait with it - in the browser
	// the bytes lie in a staging file that it deletes.
	int pendingImportKind;
	std::string pendingImportPath;
	std::string pendingImportName;
	std::unordered_map<uint, std::list<uint> > keyData;
};

#endif