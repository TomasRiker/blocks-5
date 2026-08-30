#ifndef _GS_SELECTLEVEL_H
#define _GS_SELECTLEVEL_H

/*** Klasse fuer die Levelauswahl ***/

#include "gamestate.h"
#include "engine.h"
#include "level.h"

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

	void loadLevel();
	int getLevelStatus(uint level);
	void updateNote();

	// Die Kampagnen des Benutzerverzeichnisses einlesen und die Liste damit
	// fuellen. Laeuft beim Betreten und noch einmal nach jedem Import.
	void listCampaigns();

#ifdef __EMSCRIPTEN__
	// Das Ergebnis des Dateidialogs abholen; einmal pro Logik-Tick.
	void pollImport();
	void showImportMessage(const char* p_text);

	// Eine Rueckmeldung zum Import steht eine Weile lang an der Stelle, wo
	// sonst der Hinweis auf offene Level steht - der einzige Textbereich, den
	// dieser Bildschirm hat.
	std::string importMessage;
	uint importMessageCounter;
#endif
};

#endif