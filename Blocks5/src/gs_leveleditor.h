#ifndef _GS_LEVELEDITOR_H
#define _GS_LEVELEDITOR_H

/*** Class for the level editor ***/

#include "gamestate.h"
#include "engine.h"

class TileSet;
class Level;
class Teleporter;
class Hint;
class Pin;

class GS_LevelEditor : public GameState
{
	friend class LevelEditorGUI;

public:
	GS_LevelEditor();
	~GS_LevelEditor();

	void onRender();
	void onUpdate();
	void onEnter(const ParameterBlock& context);
	void onLeave(const ParameterBlock& context);
	void onGetFocus();
	void onLoseFocus();
	void onAppLoseFocus();

	// Every change to the level - a stroke, a dialog, a key's command - runs
	// from beginChange() to endChange(), which makes it an undo point only if
	// the level then differs from how it began: an action that changes
	// nothing costs neither an undo step nor the redo list. cancelChange()
	// puts the level back as it began and touches neither list. Beginning a
	// change ends the one under way first.
	void beginChange();
	void endChange();
	void cancelChange();
	void undo();
	void redo();
	// Forgets the undo list and the change under way, for a level replaced
	// whole.
	void clearUndo();
	// Puts another level in the place of the current one, which is deleted,
	// and forgets every pointer into it.
	void replaceLevel(Level* p_newLevel);
	void forgetPickedObjects();
	void clearRedo();
	bool wasChanged();
	// For the test hook, which has no other way to see what a step saved.
	uint getUndoDepth() const;
	uint getRedoDepth() const;
	void setSavePoint();

	void setMode(int mode, bool updateRadioButtons = true);
	void setCat(int cat);
	void draw(const Vec2i& where, bool shift = false);
	void erase(const Vec2i& where, bool shift = false);
	void clear(const Vec2i& where, bool allLayers = false);
	void modify(const Vec2i& where, int buttons, bool shift, bool press);
	void transition(const Vec2i& where);
	bool copy();
	bool paste(const Vec2i& where);
	bool clear();

private:
	struct FieldInClipboard
	{
		uint tile[2];
		std::list<std::string> objectTypes;
		std::list<TiXmlElement> objectAttributes;
	};

	uint tileAtCat0(const Vec2i& where);
	void pushUndoPoint(TiXmlDocument* p_doc);
	bool levelDiffers(const TiXmlDocument* p_doc);

	Engine& engine;
	Level* p_level;

	Level* p_cat[5];
	int currentMode;
	int currentLayer;
	int currentCat;
	Level* p_currentCat;
	Vec3i currentBrush;
	bool pipetteUsed;
	std::string pipetteObjectType;
	TiXmlElement pipetteObjectAttributes;
	uint pipetteTileID;
	int oldMode;
	Vec2i rectStart;
	Vec2i rectEnd;
	int drawStartButtons;
	// The level as it was when the change under way began, 0 while none is.
	TiXmlDocument* p_changeBefore;
	Vec2i clipboardSize;
	FieldInClipboard* p_clipboard;

	Teleporter* p_teleporter;
	Hint* p_hint;
	Pin* p_currentPin;
	Pin* p_startPin;

	std::list<TiXmlDocument*> undoList;
	std::list<TiXmlDocument*> redoList;
	std::string lastSavedXML;
	std::string originalFilename;

};

#endif