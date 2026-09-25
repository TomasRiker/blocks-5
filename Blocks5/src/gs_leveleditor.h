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

	void createUndoPoint();
	// An undo point from a copy of the level taken before a change that may
	// not happen, for where the copy is thrown away if it does not: a
	// createUndoPoint() taken back would have cleared the redo list and, with
	// the list full, dropped its oldest step.
	void pushUndoPoint(TiXmlDocument* p_before);
	void undo();
	void rollback();
	void redo();
	void clearUndo();
	// Puts another level in the place of the current one, which is deleted,
	// and forgets every pointer into it.
	void replaceLevel(Level* p_newLevel);
	void forgetPickedObjects();
	void clearRedo();
	void deleteLastUndoPoint();
	bool wasChanged();
	void setSavePoint();

	void setMode(int mode, bool updateRadioButtons = true);
	void setCat(int cat);
	void draw(const Vec2i& where, bool shift = false);
	void erase(const Vec2i& where, bool shift = false);
	void clear(const Vec2i& where, bool allLayers = false);
	bool modify(const Vec2i& where, int buttons, bool shift, bool press);
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
	// Whether the stroke under way has an undo point yet: cleared at every
	// press in the level, set by every undo point made.
	bool strokeHasUndoPoint;
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