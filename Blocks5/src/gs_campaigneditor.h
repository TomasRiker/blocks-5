#ifndef _GS_CAMPAIGNEDITOR_H
#define _GS_CAMPAIGNEDITOR_H

/*** Klasse für den Kampagnen-Editor ***/

#include "gamestate.h"
#include "engine.h"

class Level;
class Campaign;
class Texture;

class GS_CampaignEditor : public GameState
{
	friend class CampaignEditorGUI;

public:
	GS_CampaignEditor();
	~GS_CampaignEditor();

	void onRender();
	void onUpdate();
	void onEnter(const ParameterBlock& context);
	void onLeave(const ParameterBlock& context);
	void onGetFocus();
	void onLoseFocus();

	bool wasChanged();
	void setSavePoint();

private:
	Engine& engine;

	Campaign* p_campaign;

	std::string lastSavedXML;
	std::string originalFilename;

	Texture* p_background;

	std::string messageText;
	uint messageCounter;
	uint messageType;
};

#endif