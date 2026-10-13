#include "pch.h"
#include "gs_leveleditor.h"
#include "transfer.h"
#include "gui_all.h"
#include "level.h"
#include "tileset.h"
#include "presets.h"
#include "object.h"
#include "teleporter.h"
#include "hint.h"
#include "electronics.h"
#include "pin.h"
#include "player.h"
#include "cf_all.h"
#include "filesystem.h"
#include "help.h"
#include "options.h"
#ifdef __EMSCRIPTEN__
#include "web_transfer.h"
#endif

class LevelEditorGUI : public GUI_Element, public sigslot::has_slots<>
{
public:
	LevelEditorGUI(GS_LevelEditor& editor) : GUI_Element("LevelEditor", 0, Vec2i(0, 0), Vec2i(640, 480)), editor(editor)
	{
		load("leveleditor.xml");

		// create the tooltip elements
		for(int x = 0; x < 24; x++)
		{
			for(int y = 0; y < 3; y++)
			{
				char name[256] = "";
				sprintf(name, "ToolTip_%d_%d", x, y);
				p_toolTip[x][y] = new GUI_Element(name, this, Vec2i(245 + x * 16, 428 + y * 16), Vec2i(16, 16));
				p_toolTip[x][y]->setToolTipOnly(true);
			}
		}

		static_cast<GUI_Button*>(getChild("Layer"))->connectClicked(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_Button*>(getChild("NumDiamondsNeeded-"))->connectClicked(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_Button*>(getChild("NumDiamondsNeeded+"))->connectClicked(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_CheckBox*>(getChild("ElectricityOn"))->connectChanged(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_Button*>(getChild("Refresh"))->connectClicked(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_Button*>(getChild("Undo"))->connectClicked(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_Button*>(getChild("Redo"))->connectClicked(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_Button*>(getChild("ShowSettings"))->connectClicked(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_Button*>(getChild("ShowMenu"))->connectClicked(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_RadioButton*>(getChild("Cat0"))->connectChanged(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_RadioButton*>(getChild("Cat1"))->connectChanged(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_RadioButton*>(getChild("Cat2"))->connectChanged(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_RadioButton*>(getChild("Cat3"))->connectChanged(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_RadioButton*>(getChild("Cat4"))->connectChanged(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_RadioButton*>(getChild("Cat0"))->check();
		static_cast<GUI_RadioButton*>(getChild("Mode0"))->connectChanged(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_RadioButton*>(getChild("Mode1"))->connectChanged(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_RadioButton*>(getChild("Mode2"))->connectChanged(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_RadioButton*>(getChild("Mode3"))->connectChanged(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_RadioButton*>(getChild("Mode4"))->connectChanged(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_RadioButton*>(getChild("Mode5"))->connectChanged(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_RadioButton*>(getChild("Mode6"))->connectChanged(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_RadioButton*>(getChild("Mode0"))->check();

		static_cast<GUI_Button*>(getChild("MenuPane.Menu.Search"))->connectClicked(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_Button*>(getChild("MenuPane.Menu.Load"))->connectClicked(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_Button*>(getChild("MenuPane.Menu.Save"))->connectClicked(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_Button*>(getChild("MenuPane.Menu.New"))->connectClicked(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_Button*>(getChild("MenuPane.Menu.Clear"))->connectClicked(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_Button*>(getChild("MenuPane.Menu.Play"))->connectClicked(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_Button*>(getChild("MenuPane.Menu.OK"))->connectClicked(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_Button*>(getChild("MenuPane.Menu.Quit"))->connectClicked(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_Button*>(getChild("MenuPane.Options"))->connectClicked(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_Button*>(getChild("MenuPane.Help"))->connectClicked(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_Button*>(getChild("MenuPane.Quit"))->connectClicked(this, &LevelEditorGUI::handleClick);

		static_cast<GUI_CheckBox*>(getChild("SettingsPane.Settings.NightVision"))->connectChanged(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_CheckBox*>(getChild("SettingsPane.Settings.Rain"))->connectChanged(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_CheckBox*>(getChild("SettingsPane.Settings.Clouds"))->connectChanged(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_CheckBox*>(getChild("SettingsPane.Settings.Thunderstorm"))->connectChanged(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_CheckBox*>(getChild("SettingsPane.Settings.Snow"))->connectChanged(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_ScrollBar*>(getChild("SettingsPane.Settings.LightColorR"))->connectChanged(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_ScrollBar*>(getChild("SettingsPane.Settings.LightColorG"))->connectChanged(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_ScrollBar*>(getChild("SettingsPane.Settings.LightColorB"))->connectChanged(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_Button*>(getChild("SettingsPane.Settings.OK"))->connectClicked(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_Button*>(getChild("SettingsPane.Settings.Cancel"))->connectClicked(this, &LevelEditorGUI::handleClick);

		static_cast<GUI_Button*>(getChild("EditHintPane.EditHint.OK"))->connectClicked(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_Button*>(getChild("EditHintPane.EditHint.Cancel"))->connectClicked(this, &LevelEditorGUI::handleClick);

		static_cast<GUI_Button*>(getChild("MessageBoxPane.MessageBox.Yes"))->connectClicked(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_Button*>(getChild("MessageBoxPane.MessageBox.No"))->connectClicked(this, &LevelEditorGUI::handleClick);

		static_cast<GUI_Button*>(getChild("SearchPane.Search.Select"))->connectClicked(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_Button*>(getChild("SearchPane.Search.Cancel"))->connectClicked(this, &LevelEditorGUI::handleClick);

		oldCursor = Vec2i(-1, -1);
		confirmed = false;
		p_clickWhenConfirmed = 0;
		searchLoads = false;
		realDown = true;

		p_options = new Options(this);
		p_help = new Help(this);
	}

	~LevelEditorGUI()
	{
		delete p_options;
		delete p_help;
	}

	void onUpdate()
	{
		char s[256];

		sprintf(s, "%s: %d", localizeString("$LE_LAYER").c_str(), editor.currentLayer);
		static_cast<GUI_Button*>(getChild("Layer"))->setTitle(s);

		sprintf(s, "%s: %d", localizeString("$LE_DIAMONDS").c_str(), editor.p_level->getNumDiamondsNeeded());
		static_cast<GUI_StaticText*>(getChild("NumDiamondsNeeded"))->setText(s);

		// setChecked, not check(): the box catching up with the level is not a
		// click and must not fire changed.
		static_cast<GUI_CheckBox*>(getChild("ElectricityOn"))->setChecked(editor.p_level->isElectricityOn());
	}

	// Where onMouseDown below does something: the level and the palette. A
	// finger there paints or picks exactly where it is; one that misses a
	// button on the rest of the toolbar is moved onto the button.
	bool isClickTarget(const Vec2i& position)
	{
		return position.y < 400 || (position.y >= 428 && position.x >= 245);
	}

	void onMouseDown(const Vec2i& position,
					 int buttons)
	{
		// A press starts a stroke, and onMouseMove interpolates from oldCursor.
		// A finger makes no moves between strokes, so without this a stroke
		// would begin with a line from where the last one ended. Here and not
		// in onMouseUp, which a cancelled touch never sends; and only on a
		// real press, since the interpolation calls this function too.
		if(realDown) oldCursor = position / 16;

		bool shift = editor.engine.isKeyDown(SDLK_LSHIFT) || editor.engine.isKeyDown(SDLK_RSHIFT);

		if(position.y < 400)
		{
			Vec2i p = position / 16;

			// The gaps a drag fills in come through here as well (realDown
			// false), and belong to the stroke's change. Where that has ended -
			// cancelled by the other button, undone, cut short by a lost focus
			// - they change nothing.
			if(!realDown && !editor.p_changeBefore &&
			   (editor.currentMode == 0 || editor.currentMode == 1 || editor.currentMode == 3)) return;

			if(editor.currentMode == 0)
			{
				if(realDown)
				{
					// Is this an attempt to cancel?
					if(editor.drawStartButtons && editor.drawStartButtons != buttons)
					{
						// Yes.
						editor.cancelChange();
						editor.drawStartButtons = 0;
						buttons = 0;
					}
					else
					{
						editor.drawStartButtons = buttons;
						editor.beginChange();
					}
				}

				if(buttons & 1)
				{
					// apply the pen
					editor.draw(p, shift);
				}
				else if(buttons & 3)
				{
					// eraser
					editor.erase(p, shift);
				}
			}
			else if(editor.currentMode == 1)
			{
				if(buttons & 1)
				{
					// The stroke is one change, from the press to the release.
					if(realDown) editor.beginChange();

					if(editor.p_teleporter)
					{
						// set the teleporter's target position
						editor.p_teleporter->setTargetPosition(p);
					}
					else editor.modify(p, buttons, shift, realDown);
				}
			}
			else if(editor.currentMode == 2 || editor.currentMode == 4)
			{
				if(realDown)
				{
					// Is this an attempt to cancel?
					if(editor.drawStartButtons && editor.drawStartButtons != buttons)
					{
						// Yes.
						editor.rectStart = editor.rectEnd = Vec2i(-1, -1);
						editor.drawStartButtons = 0;
					}
					else
					{
						// set the rectangle's start point and remember which mouse button is down
						editor.rectStart = p;
						editor.drawStartButtons = buttons;
					}
				}

				editor.rectEnd = p;
			}
			else if(editor.currentMode == 3)
			{
				if(realDown)
				{
					// Is this an attempt to cancel?
					if(editor.drawStartButtons && editor.drawStartButtons != buttons)
					{
						// Yes.
						editor.cancelChange();
						editor.drawStartButtons = 0;
						buttons = 0;
					}
					else
					{
						editor.drawStartButtons = buttons;
						editor.beginChange();
					}
				}

				if(buttons) editor.transition(position, realDown);
			}
			else if(editor.currentMode == 5)
			{
				if(buttons & 1)
				{
					editor.pipetteTileID = 0;
					editor.pipetteObjectType = "";
					editor.pipetteUsed = true;

					// Is there an object here?
					Object* p_object = editor.p_level->getFrontObjectAt(p);
					if(p_object)
					{
						// remember the type and the attributes
						editor.pipetteObjectType = p_object->getType();
						editor.pipetteObjectAttributes = TiXmlElement("");
						p_object->saveAttributes(&editor.pipetteObjectAttributes);
					}
					else
					{
						// Which tile is on the current layer here?
						editor.pipetteTileID = editor.p_level->getTileAt(editor.currentLayer, p);
					}
				}
			}
			else if(editor.currentMode == 6 && realDown)
			{
				// Only the press: pins are clicked, not painted, and the
				// drag's gap filling would connect the pin just picked to
				// itself. The pin is looked for under the press itself: a
				// finger lands with no hover before it, and a mouse may have
				// moved in this tick.
				editor.p_currentPin = pinAt(position);
				if(editor.p_currentPin)
				{
					if(buttons & 1)
					{
						if(editor.p_startPin)
						{
							// connect
							editor.beginChange();
							const bool r = Pin::connect(editor.p_startPin, editor.p_currentPin);
							editor.endChange();
							if(!r) Engine::inst().showToast(Engine::TOAST_ERROR, "$LE_ERROR_INVALID_CONNECTION");

							editor.p_currentPin = editor.p_startPin = 0;
						}
						else editor.p_startPin = editor.p_currentPin;
					}
					else if(buttons & 3)
					{
						if(editor.p_currentPin->isConnected())
						{
							// disconnect all of this pin's connections
							editor.beginChange();
							editor.p_currentPin->disconnectAll();
							editor.endChange();
							editor.p_currentPin = editor.p_startPin = 0;
						}
					}
				}
				else
				{
					// Off a pin, either button lets go of a connection begun.
					if(buttons & 3)
					{
						editor.p_currentPin = editor.p_startPin = 0;
					}
				}
			}
		}
		else if(position.y >= 428 && position.x >= 245)
		{
			// A new tile or object has probably been selected.
			Vec2i p = (position - Vec2i(245, 428)) / 16;

			// Is there anything at all in the current category?
			uint l0 = editor.p_currentCat->getTileAt(0, p);
			uint l1 = editor.p_currentCat->getTileAt(1, p);
			const TileSet::TileInfo& i0 = editor.p_currentCat->getTileSet()->getTileInfo(l0);
			const TileSet::TileInfo& i1 = editor.p_currentCat->getTileSet()->getTileInfo(l1);
			if(i0.type == -1 && i1.type == -1) l0 = l1 = 0;
			Object* p_obj = editor.p_currentCat->getFrontObjectAt(p);

			if(l0 || l1 || p_obj)
			{
				// The selection is valid.
				editor.currentBrush = Vec3i(p.x, p.y, editor.currentCat);
				editor.pipetteUsed = false;
				if(editor.currentMode != 0 && editor.currentMode != 2) editor.setMode(0);
			}
		}
	}

	void onMouseUp(const Vec2i& position,
				   int buttons)
	{
		bool shift = editor.engine.isKeyDown(SDLK_LSHIFT) || editor.engine.isKeyDown(SDLK_RSHIFT);

		editor.p_teleporter = 0;

		// The stroke's change ends with it - but not one a dialog holds, the
		// note being written or the settings, which end with OK or Cancel.
		if(!dialogOpen()) editor.endChange();

		if(editor.currentMode == 2 || editor.currentMode == 4)
		{
			// order the rectangle's corners from top left to bottom right
			Vec2i pMin(min(editor.rectStart.x, editor.rectEnd.x), min(editor.rectStart.y, editor.rectEnd.y));
			Vec2i pMax(max(editor.rectStart.x, editor.rectEnd.x), max(editor.rectStart.y, editor.rectEnd.y));
			editor.rectStart = pMin;
			editor.rectEnd = pMax;
		}

		if(editor.currentMode == 2)
		{
			// Draw the rectangle - only one whose stroke is still under way: a
			// lost focus ends it, and the release that comes later belongs to
			// another press, a palette click as like as not.
			if(editor.rectStart.x != -1 && editor.drawStartButtons)
			{
				editor.beginChange();

				for(int x = editor.rectStart.x; x <= editor.rectEnd.x; x++)
				{
					for(int y = editor.rectStart.y; y <= editor.rectEnd.y; y++)
					{
						if(buttons & 1) editor.draw(Vec2i(x, y), shift);
						else if(buttons & 3) editor.clear(Vec2i(x, y));
					}
				}

				editor.endChange();
			}
			editor.rectStart = editor.rectEnd = Vec2i(-1, -1);
		}

		if(editor.currentMode == 5)
		{
			// go into draw mode
			editor.setMode(0);
		}

		if(editor.oldMode != -1)
		{
			editor.setMode(editor.oldMode);
			editor.oldMode = -1;
		}

		editor.drawStartButtons = 0;
	}

	void onMouseMove(const Vec2i& position,
					 const Vec2i& movement,
					 int buttons)
	{
		Vec2i p = position / 16;

		if(position.y < 400)
		{
			if(buttons)
			{
				if(GUI::inst().getMouseDownElement() != this)
				{
					oldCursor = Vec2i(-1, -1);
					return;
				}

				realDown = false;

				if(oldCursor == Vec2i(-1, -1)) oldCursor = p;
				std::vector<Vec2i> line = bresenham(oldCursor, p);
				for(uint i = 1; i < line.size(); i++)
				{
					onMouseDown(line[i] * 16, buttons);
				}

				realDown = true;
			}
			else
			{
				// The pin under the cursor lights up.
				if(editor.currentMode == 6) editor.p_currentPin = pinAt(position);
			}
		}

		oldCursor = p;
	}

	void onKeyEvent(const SDL_KeyboardEvent& event)
	{
		// Every key here is a command, so a repeat is dropped: a held Escape
		// would open and close the menu over and over.
		if(GUI::inst().isKeyRepeat()) return;

		// The dialogs on top come first, Escape and Return meaning Cancel and
		// OK there.
		if(event.type == SDL_KEYDOWN)
		{
			if(getChild("SettingsPane")->isVisible())
			{
				if(event.keysym.sym == SDLK_ESCAPE)
				{
					handleClick(getChild("SettingsPane.Settings.Cancel"));
					return;
				}
				if(isReturnKey(event.keysym.sym))
				{
					handleClick(getChild("SettingsPane.Settings.OK"));
					return;
				}
			}
			else if(getChild("EditHintPane")->isVisible())
			{
				// Escape cancels the hint dialog; Return stays a line break in its
				// text box.
				if(event.keysym.sym == SDLK_ESCAPE)
				{
					handleClick(getChild("EditHintPane.EditHint.Cancel"));
					return;
				}
			}
			else if(getChild("MenuPane")->isVisible() && !getChild("MessageBoxPane")->isVisible())
			{
				// Escape closes the file search where it stands over the menu,
				// and otherwise the menu, through OK, its one plain way out -
				// but not under one of its confirmations, which would be left
				// alone. The options and the help take their own Escape.
				if(event.keysym.sym == SDLK_ESCAPE)
				{
					if(getChild("SearchPane")->isVisible()) handleClick(getChild("SearchPane.Search.Cancel"));
					else handleClick(getChild("MenuPane.Menu.OK"));
					return;
				}

				// The keys its buttons show work with the menu open, wherever
				// nothing stands over it, the name box handing them on, and so
				// does F10, which reloads only the graphics. The keys that edit
				// the level or pick a tool stay shut out below: the level is
				// behind the menu, and the name box types most of them.
				if(!getChild("SearchPane")->isVisible() && !p_options->isVisible() && !p_help->isVisible())
				{
					const bool ctrl = (event.keysym.mod & KMOD_LCTRL) || (event.keysym.mod & KMOD_RCTRL);
					const char letter = ctrl ? keyLetter(event.keysym) : 0;
					GUI_Element* p_button = 0;
					if(event.keysym.sym == SDLK_F5) p_button = getChild("MenuPane.Menu.Play");
					else if(event.keysym.sym == SDLK_F10) p_button = getChild("Refresh");
					else if(letter == 's') p_button = getChild("MenuPane.Menu.Save");
					else if(letter == 'o') p_button = getChild("MenuPane.Menu.Load");
					if(p_button)
					{
						handleClick(p_button);
						return;
					}
				}
			}
		}

		// Not under a pane either: a key the menu, its options, its help or
		// the file search does not use is passed on to here, and would change
		// the level behind it.
		if(!getChild("SettingsPane")->isVisible() && !getChild("EditHintPane")->isVisible() &&
		   !getChild("MessageBoxPane")->isVisible() && !getChild("MenuPane")->isVisible() &&
		   !getChild("SearchPane")->isVisible())
		{
			// Only a key press matters here.
			if(event.type != SDL_KEYDOWN) return;

			// Shift, Ctrl pressed?
			bool shift = (event.keysym.mod & KMOD_LSHIFT) || (event.keysym.mod & KMOD_RSHIFT);
			bool ctrl = (event.keysym.mod & KMOD_LCTRL) || (event.keysym.mod & KMOD_RCTRL);

			// Letters go by the label on the key, whatever the layout
			// (keyLetter() says why the keysym cannot): Ctrl+Z undoes, Ctrl+Y
			// and Ctrl+Shift+Z redo.
			switch(keyLetter(event.keysym))
			{
			case 's':
				if(ctrl) handleClick(getChild("MenuPane.Menu.Save"));
				break;
			case 'o':
				// Asks which level first, as Ctrl+O does everywhere: the menu
				// opens with its file list, and Select there loads. With the
				// menu open, the key is its Load button's.
				if(ctrl)
				{
					openMenu();
					handleClick(getChild("MenuPane.Menu.Search"));
					searchLoads = true;
				}
				break;
			case 'l':
				handleClick(getChild("Layer"));
				break;
			case 'z':
				if(ctrl)
				{
					if(shift) editor.redo();
					else editor.undo();
				}
				break;
			case 'y':
				if(ctrl) editor.redo();
				break;
			case 'c':
				if(ctrl) editor.copy();
				break;
			case 'v':
				if(ctrl)
				{
					editor.beginChange();
					editor.paste(editor.rectStart);
					editor.endChange();
				}
				break;
			case 'x':
				if(ctrl)
				{
					if(!editor.copy()) break;
					editor.beginChange();
					editor.clear();
					editor.endChange();
				}
				break;
			}

			switch(event.keysym.sym)
			{
			case SDLK_ESCAPE:
				openMenu();
				break;
			case SDLK_F5:
				handleClick(getChild("MenuPane.Menu.Play"));
				break;
			case SDLK_F10:
				handleClick(getChild("Refresh"));
				break;
			case SDLK_1: if(shift) static_cast<GUI_RadioButton*>(getChild("Cat0"))->check(); else editor.setMode(0); break;
			case SDLK_2: if(shift) static_cast<GUI_RadioButton*>(getChild("Cat1"))->check(); else editor.setMode(1); break;
			case SDLK_3: if(shift) static_cast<GUI_RadioButton*>(getChild("Cat2"))->check(); else editor.setMode(2); break;
			case SDLK_4: if(shift) static_cast<GUI_RadioButton*>(getChild("Cat3"))->check(); else editor.setMode(3); break;
			case SDLK_5: if(shift) static_cast<GUI_RadioButton*>(getChild("Cat4"))->check(); else editor.setMode(4); break;
			case SDLK_6: if(!shift) editor.setMode(5); break;
			case SDLK_7: if(!shift) editor.setMode(6); break;
			case SDLK_DELETE:
				editor.beginChange();
				editor.clear();
				editor.endChange();
				break;
			case SDLK_LEFT:
			case SDLK_RIGHT:
			case SDLK_UP:
			case SDLK_DOWN:
				{
					Vec2i dir(0, 0);
					switch(event.keysym.sym)
					{
					case SDLK_LEFT: dir = Vec2i(-1, 0); break;
					case SDLK_RIGHT: dir = Vec2i(1, 0); break;
					case SDLK_UP: dir = Vec2i(0, -1); break;
					case SDLK_DOWN: dir = Vec2i(0, 1); break;
					}

					if(editor.currentMode == 4)
					{
						if(editor.rectStart.x != -1)
						{
							Vec2i pMin(min(editor.rectStart.x, editor.rectEnd.x), min(editor.rectStart.y, editor.rectEnd.y));

							GS_LevelEditor::FieldInClipboard* p_oldClipboard = editor.p_clipboard;
							Vec2i oldClipboardSize = editor.clipboardSize;

							editor.clipboardSize = Vec2i(0, 0);
							editor.p_clipboard = 0;

							editor.beginChange();
							bool r = editor.copy();
							r &= editor.clear();
							r &= editor.paste(pMin + dir);

							delete[] editor.p_clipboard;
							editor.p_clipboard = p_oldClipboard;
							editor.clipboardSize = oldClipboardSize;

							if(r) editor.endChange();
							else editor.cancelChange();
						}
					}
					else
					{
						Vec2i p = editor.engine.getCursorPosition() / 16;
						if(p.x < Level::WIDTH && p.y < Level::HEIGHT)
						{
							Object* p_obj = editor.p_level->getFrontObjectAt(p);
							if(p_obj)
							{
								Vec2i np = p + dir;
								if(np.x >= 0 && np.y >= 0 && np.x < Level::WIDTH && np.y < Level::HEIGHT)
								{
									editor.beginChange();

									// What stands in the target cell is deleted
									// below, and a wire's pins or the teleporter
									// being aimed may belong to it.
									editor.forgetPickedObjects();

									if(shift)
									{
										// copy the object
										TiXmlElement e("");
										p_obj->saveAttributes(&e);
										if(p_obj->getType() != "Elevator" && !ctrl)
										{
											editor.p_level->clearPosition(p + dir);
											editor.p_level->removeOldObjects();
										}

										editor.p_level->getPresets()->instancePreset(p_obj->getType(), p + dir, &e);
									}
									else
									{
										if(p_obj->getType() != "Elevator" && !ctrl)
										{
											editor.p_level->clearPosition(p + dir);
											editor.p_level->removeOldObjects();
										}

										p_obj->warpTo(p + dir);
									}

									editor.endChange();
									editor.engine.setCursorPosition(editor.engine.getCursorPosition() + 16 * dir);
								}
							}
						}
					}
				}
				break;
			}
		}
	}

	void handleClick(GUI_Element* p_element)
	{
		const std::string& name = p_element->getFullName();
		if(name == "LevelEditor.Layer")
		{
			editor.currentLayer++;
			editor.currentLayer %= Level::NUM_LAYERS;
		}
		else if(name == "LevelEditor.NumDiamondsNeeded-")
		{
			const int n = editor.p_level->getNumDiamondsNeeded();
			if(n)
			{
				editor.beginChange();
				editor.p_level->setNumDiamondsNeeded(n - 1);
				editor.endChange();
			}
		}
		else if(name == "LevelEditor.NumDiamondsNeeded+")
		{
			editor.beginChange();
			editor.p_level->setNumDiamondsNeeded(editor.p_level->getNumDiamondsNeeded() + 1);
			editor.endChange();
		}
		else if(name == "LevelEditor.ElectricityOn")
		{
			editor.beginChange();
			editor.p_level->setElectricityOn(static_cast<GUI_CheckBox*>(p_element)->isChecked());
			editor.endChange();
		}
		else if(name == "LevelEditor.Refresh")
		{
			editor.p_level->loadSkin(true);
			for(int i = 0; i < 5; i++) editor.p_cat[i]->loadSkin();

			TiXmlDocument* p_doc = editor.p_level->save();
			Level* p_newLevel = new Level;
			p_newLevel->setInEditor(true);
			p_newLevel->load(p_doc);
			delete p_doc;
			editor.replaceLevel(p_newLevel);

			Engine::inst().showToast(Engine::TOAST_OK, "$LE_INFO_GRAPHICS_RELOADED");
		}
		else if(name == "LevelEditor.Undo") editor.undo();
		else if(name == "LevelEditor.Redo") editor.redo();
		else if(name == "LevelEditor.ShowSettings")
		{
			static_cast<GUI_EditBox*>(getChild("SettingsPane.Settings.Title"))->setText(editor.p_level->getTitle());

			static_cast<GUI_CheckBox*>(getChild("SettingsPane.Settings.NightVision"))->setChecked(editor.p_level->isNightVision());
			static_cast<GUI_CheckBox*>(getChild("SettingsPane.Settings.Rain"))->setChecked(editor.p_level->isRaining());
			static_cast<GUI_CheckBox*>(getChild("SettingsPane.Settings.Clouds"))->setChecked(editor.p_level->isCloudy());
			static_cast<GUI_CheckBox*>(getChild("SettingsPane.Settings.Snow"))->setChecked(editor.p_level->isSnowing());
			static_cast<GUI_CheckBox*>(getChild("SettingsPane.Settings.Thunderstorm"))->setChecked(editor.p_level->isThunderstorm());

			const Vec3i& lightColor = editor.p_level->getLightColor();
			static_cast<GUI_ScrollBar*>(getChild("SettingsPane.Settings.LightColorR"))->setScroll(lightColor.r);
			static_cast<GUI_ScrollBar*>(getChild("SettingsPane.Settings.LightColorG"))->setScroll(lightColor.g);
			static_cast<GUI_ScrollBar*>(getChild("SettingsPane.Settings.LightColorB"))->setScroll(lightColor.b);

			static_cast<GUI_EditBox*>(getChild("SettingsPane.Settings.MusicFilename"))->setText(editor.p_level->getMusicFilename());

			for(uint i = 0; i < Level::SKIN_MAX; i++)
			{
				char elementName[256] = "";
				sprintf(elementName, "SettingsPane.Settings.Skin%d", i);
				static_cast<GUI_EditBox*>(getChild(elementName))->setText(editor.p_level->getSkin(i));
			}

			// The dialog changes the level as its controls move, and OK or
			// Cancel ends the change.
			getChild("SettingsPane.Settings")->focus();
			editor.beginChange();
		}
		else if(name == "LevelEditor.ShowMenu")
		{
			openMenu();
		}
		else if(name == "LevelEditor.Cat0") editor.setCat(0);
		else if(name == "LevelEditor.Cat1") editor.setCat(1);
		else if(name == "LevelEditor.Cat2") editor.setCat(2);
		else if(name == "LevelEditor.Cat3") editor.setCat(3);
		else if(name == "LevelEditor.Cat4") editor.setCat(4);
		else if(name == "LevelEditor.Mode0") editor.setMode(0, false);
		else if(name == "LevelEditor.Mode1") editor.setMode(1, false);
		else if(name == "LevelEditor.Mode2") editor.setMode(2, false);
		else if(name == "LevelEditor.Mode3") editor.setMode(3, false);
		else if(name == "LevelEditor.Mode4") editor.setMode(4, false);
		else if(name == "LevelEditor.Mode5") editor.setMode(5, false);
		else if(name == "LevelEditor.Mode6") editor.setMode(6, false);

		if(name == "LevelEditor.MenuPane.Menu.New")
		{
			if(!editor.wasChanged() || confirmed)
			{
				editor.clearUndo();
				editor.clearRedo();

				editor.p_level->load("level_default.xml");
				editor.setSavePoint();
				static_cast<GUI_EditBox*>(getChild("MenuPane.Menu.Filename"))->setText("");
				getChild("MenuPane")->hide();
				focus();

				editor.originalFilename = "";
				editor.forgetPickedObjects();
				editor.setMode(0);
			}
			else
			{
				getChild("MessageBoxPane.MessageBox.Text1")->show();
				getChild("MessageBoxPane.MessageBox.Text2")->hide();
				getChild("MessageBoxPane.MessageBox")->focus();
				p_clickWhenConfirmed = p_element;
			}
		}
		else if(name == "LevelEditor.MenuPane.Menu.Clear")
		{
			editor.beginChange();
			editor.p_level->clean();
			editor.forgetPickedObjects();
			editor.endChange();

			getChild("MenuPane")->hide();
			focus();
		}
		else if(name == "LevelEditor.MenuPane.Menu.Play")
		{
			getChild("MenuPane")->hide();
			focus();

			ParameterBlock p;
			p.set("levelDocument", editor.p_level->save());
			p.set("levelUnsaved", editor.wasChanged());
			editor.engine.pushGameState("GS_Game", p);
			editor.engine.crossfade(new CF_Mosaic, 0.85f);
		}
		else if(name == "LevelEditor.MenuPane.Options")
		{
			getChild("MenuPane.Menu")->hide();
			p_options->show(getChild("MenuPane.Menu"));
		}
		else if(name == "LevelEditor.MenuPane.Help")
		{
			getChild("MenuPane.Menu")->hide();
			p_help->show(getChild("MenuPane.Menu"));
		}
		else if(name == "LevelEditor.MenuPane.Menu.Search")
		{
			// Picking a level names it in the box, ready for Load or Save.
			// Ctrl+O and a Load with no name set this afterwards, and then
			// the pick loads.
			searchLoads = false;

			// Fill the file list from both roots: the example levels sit with
			// the game, the player's own with the player.
			const std::vector<std::string> files(Transfer::list(Transfer::KIND_LEVEL));
			GUI_ListBox* p_listBox = static_cast<GUI_ListBox*>(getChild("SearchPane.Search.Files"));
			p_listBox->clear();
			for(std::vector<std::string>::const_iterator i = files.begin(); i != files.end(); ++i)
			{
				GUI_ListBox::ListItem item(*i, 0);
				p_listBox->addItem(item);
			}

			// The list takes the keys, the level named in the box picked, so
			// the arrows and Return work from the start.
			const std::string current(static_cast<GUI_EditBox*>(getChild("MenuPane.Menu.Filename"))->getText());
			p_listBox->setSelection(p_listBox->findItem(setFilenameExtension(current, "xml")));
			p_listBox->focus();
		}
		else if(name == "LevelEditor.MenuPane.Menu.Load")
		{
			std::string filename = static_cast<GUI_EditBox*>(getChild("MenuPane.Menu.Filename"))->getText();
			std::string path;
			if(!filename.empty())
			{
				path = FileSystem::inst().resolveContentPath("levels/" + setFilenameExtension(filename, "xml"));

				if(FileSystem::inst().fileExists(path))
				{
					if(!editor.wasChanged() || confirmed)
					{
						Level* p_newLevel = new Level;
						p_newLevel->setInEditor(true);

						if(!p_newLevel->load(path))
						{
							path = "";
							delete p_newLevel;
							static_cast<GUI_EditBox*>(getChild("MenuPane.Menu.Filename"))->setText("");
						}
						else
						{
							editor.replaceLevel(p_newLevel);

							getChild("MenuPane")->hide();
							focus();

							editor.setSavePoint();

							editor.clearUndo();
							editor.clearRedo();

							editor.originalFilename = path;
							editor.setMode(0);
						}
					}
					else
					{
						getChild("MessageBoxPane.MessageBox.Text1")->show();
						getChild("MessageBoxPane.MessageBox.Text2")->hide();
						getChild("MessageBoxPane.MessageBox")->focus();
						p_clickWhenConfirmed = p_element;
					}
				}
				else
				{
					Engine::inst().showToast(Engine::TOAST_ERROR, "$LE_ERROR_FILE_DOESNT_EXIST");
				}
			}
			else
			{
				// No filename: the file list asks for one, and the level
				// picked there loads, as after Ctrl+O from the level.
				handleClick(getChild("MenuPane.Menu.Search"));
				searchLoads = true;
			}
		}
		else if(name == "LevelEditor.MenuPane.Menu.Save")
		{
			std::string filename = static_cast<GUI_EditBox*>(getChild("MenuPane.Menu.Filename"))->getText();
			std::string path;
			if(!filename.empty())
			{
				const std::string basename(setFilenameExtension(filename, "xml"));

				// Saving goes to the user directory, and never under a shipped
				// name: the game folder answers first, so such a file could
				// never be loaded again.
				if(Transfer::isBuiltIn(Transfer::KIND_LEVEL, basename))
				{
					Engine::inst().showToast(Engine::TOAST_ERROR, "$TR_ERROR_RESERVED");
					return;
				}

				path = FileSystem::inst().getAppHomeDirectory() + "levels/" + basename;

				bool doSave = true;
				if(path != editor.originalFilename && FileSystem::inst().fileExists(path))
				{
					if(!confirmed)
					{
						getChild("MessageBoxPane.MessageBox.Text1")->hide();
						getChild("MessageBoxPane.MessageBox.Text2")->show();
						getChild("MessageBoxPane.MessageBox")->focus();
						p_clickWhenConfirmed = p_element;
						doSave = false;
					}
				}

				if(doSave)
				{
					if(editor.p_level->save(path))
					{
						editor.setSavePoint();
						getChild("MenuPane")->hide();
						focus();

						editor.originalFilename = path;
#ifdef __EMSCRIPTEN__
						// Straight into the IndexedDB, not waiting for the next
						// five-second interval.
						WebTransfer::syncHome();
#endif

						Engine::inst().showToast(Engine::TOAST_OK, "$LE_INFO_LEVEL_SAVED");
					}
					else
					{
						Engine::inst().showToast(Engine::TOAST_ERROR, "$LE_ERROR_SAVING");
					}
				}
			}
			else
			{
				// No filename: the cursor goes where one belongs, and Return
				// there saves. From the level, which Ctrl+S saves from, that
				// opens the menu, the way Save asks for a name for an untitled
				// file in other programs, and only in the menu is it an error.
				if(getChild("MenuPane")->isVisible()) Engine::inst().showToast(Engine::TOAST_ERROR, "$ERROR_NO_FILENAME");
				GUI_EditBox* p_box = static_cast<GUI_EditBox*>(getChild("MenuPane.Menu.Filename"));
				p_box->setSubmitButton(static_cast<GUI_Button*>(getChild("MenuPane.Menu.Save")));
				p_box->focus();
			}
		}
		else if(name == "LevelEditor.MenuPane.Menu.OK")
		{
			getChild("MenuPane")->hide();
			focus();
		}
		else if(name == "LevelEditor.MenuPane.Menu.Quit")
		{
			if(!editor.wasChanged() || confirmed)
			{
				editor.engine.crossfade(new CF_Star, 0.85f);
				editor.engine.popGameState();
			}
			else
			{
				getChild("MessageBoxPane.MessageBox.Text1")->show();
				getChild("MessageBoxPane.MessageBox.Text2")->hide();
				getChild("MessageBoxPane.MessageBox")->focus();
				p_clickWhenConfirmed = p_element;
			}
		}
		else if(name == "LevelEditor.MenuPane.Quit")
		{
			// Leaving the game throws the level away as surely as leaving the
			// editor does, so it asks the same question.
			if(!editor.wasChanged() || confirmed)
			{
				SDL_Event event;
				event.type = SDL_QUIT;
				SDL_PushEvent(&event);
			}
			else
			{
				getChild("MessageBoxPane.MessageBox.Text1")->show();
				getChild("MessageBoxPane.MessageBox.Text2")->hide();
				getChild("MessageBoxPane.MessageBox")->focus();
				p_clickWhenConfirmed = p_element;
			}
		}

		if(name == "LevelEditor.SettingsPane.Settings.NightVision")
		{
			bool on = static_cast<GUI_CheckBox*>(p_element)->isChecked();
			editor.p_level->setNightVision(on);
		}
		else if(name == "LevelEditor.SettingsPane.Settings.Rain")
		{
			bool on = static_cast<GUI_CheckBox*>(p_element)->isChecked();
			editor.p_level->setRaining(on);
		}
		else if(name == "LevelEditor.SettingsPane.Settings.Clouds")
		{
			bool on = static_cast<GUI_CheckBox*>(p_element)->isChecked();
			editor.p_level->setCloudy(on);
		}
		else if(name == "LevelEditor.SettingsPane.Settings.Snow")
		{
			bool on = static_cast<GUI_CheckBox*>(p_element)->isChecked();
			editor.p_level->setSnowing(on);
		}
		else if(name == "LevelEditor.SettingsPane.Settings.Thunderstorm")
		{
			bool on = static_cast<GUI_CheckBox*>(p_element)->isChecked();
			editor.p_level->setThunderstorm(on);
		}
		else if(name == "LevelEditor.SettingsPane.Settings.LightColorR")
		{
			Vec3i lightColor = editor.p_level->getLightColor();
			lightColor.r = static_cast<GUI_ScrollBar*>(getChild("SettingsPane.Settings.LightColorR"))->getScroll();
			editor.p_level->setLightColor(lightColor);
		}
		else if(name == "LevelEditor.SettingsPane.Settings.LightColorG")
		{
			Vec3i lightColor = editor.p_level->getLightColor();
			lightColor.g = static_cast<GUI_ScrollBar*>(getChild("SettingsPane.Settings.LightColorG"))->getScroll();
			editor.p_level->setLightColor(lightColor);
		}
		else if(name == "LevelEditor.SettingsPane.Settings.LightColorB")
		{
			Vec3i lightColor = editor.p_level->getLightColor();
			lightColor.b = static_cast<GUI_ScrollBar*>(getChild("SettingsPane.Settings.LightColorB"))->getScroll();
			editor.p_level->setLightColor(lightColor);
		}
		else if(name == "LevelEditor.SettingsPane.Settings.OK")
		{
			editor.p_level->setTitle(static_cast<GUI_EditBox*>(getChild("SettingsPane.Settings.Title"))->getText());
			editor.p_level->setMusicFilename(static_cast<GUI_EditBox*>(getChild("SettingsPane.Settings.MusicFilename"))->getText());

			bool r = false;
			for(uint i = 0; i < Level::SKIN_MAX; i++)
			{
				char elementName[256] = "";
				sprintf(elementName, "SettingsPane.Settings.Skin%d", i);
				r |= editor.p_level->setSkin(i, static_cast<GUI_EditBox*>(getChild(elementName))->getText());
			}

			editor.endChange();
			getChild("SettingsPane")->hide();
			focus();

			if(r) handleClick(getChild("Refresh"));
		}
		else if(name == "LevelEditor.SettingsPane.Settings.Cancel")
		{
			editor.cancelChange();
			getChild("SettingsPane")->hide();
			focus();
		}

		if(name == "LevelEditor.EditHintPane.EditHint.OK")
		{
			editor.p_hint->setText(static_cast<GUI_MultiLineEditBox*>(getChild("EditHintPane.EditHint.Text"))->getText());
			editor.endChange();
			editor.p_hint->releaseNoteTexture();
			editor.p_hint = 0;
			getChild("EditHintPane")->hide();
			focus();

			if(editor.oldMode != -1)
			{
				editor.setMode(editor.oldMode);
				editor.oldMode = -1;
			}
		}
		else if(name == "LevelEditor.EditHintPane.EditHint.Cancel")
		{
			editor.cancelChange();
			// A change taken back replaced the level, the note with it, and
			// dropped the pointer. Otherwise the note is still there and
			// gives its preview's texture back as on OK. The pointer goes
			// either way: the preview draws the note while it is set, and the
			// note may be erased later.
			if(editor.p_hint) editor.p_hint->releaseNoteTexture();
			editor.p_hint = 0;
			getChild("EditHintPane")->hide();
			focus();
		}

		if(name == "LevelEditor.MessageBoxPane.MessageBox.Yes")
		{
			getChild("MessageBoxPane")->hide();
			focusAfterQuestion();
			confirmed = true;
			handleClick(p_clickWhenConfirmed);
			confirmed = false;
		}
		else if(name == "LevelEditor.MessageBoxPane.MessageBox.No")
		{
			// The level's name back in the box, not its path, which Load and
			// Save would put a folder in front of again.
			getChild("MessageBoxPane")->hide();
			focusAfterQuestion();
			static_cast<GUI_EditBox*>(getChild("MenuPane.Menu.Filename"))->setText(FileSystem::inst().getPathFilename(editor.originalFilename));
		}

		if(name == "LevelEditor.SearchPane.Search.Select")
		{
			GUI_ListBox* p_listBox = static_cast<GUI_ListBox*>(getChild("SearchPane.Search.Files"));
			GUI_ListBox::ListItem* p_item = p_listBox->getSelectedItem();
			if(p_item)
			{
				static_cast<GUI_EditBox*>(getChild("MenuPane.Menu.Filename"))->setText(p_item->text);
				getChild("SearchPane")->hide();
				getChild("MenuPane.Menu")->focus();
				if(searchLoads) handleClick(getChild("MenuPane.Menu.Load"));
			}
		}
		else if(name == "LevelEditor.SearchPane.Search.Cancel")
		{
			getChild("SearchPane")->hide();
			getChild("MenuPane.Menu")->focus();
		}

		if(editor.p_level)
		{
			for(int i = 0; i < 5; i++)
			{
				for(uint j = 0; j < Level::SKIN_MAX; j++)
				{
					editor.p_cat[i]->setSkin(j, editor.p_level->getSkin(j));
				}
			}
		}
	}

	// A dialog that holds the level's change under way: the note editor and
	// the settings end it themselves, with OK or Cancel.
	bool dialogOpen()
	{
		return getChild("EditHintPane")->isVisible() || getChild("SettingsPane")->isVisible();
	}

	// A question's window takes the focus with it as it goes, which leaves
	// the keys to nobody: they go back to the menu it was asked over, or to
	// the level where Ctrl+S asked it with the menu closed.
	void focusAfterQuestion()
	{
		if(getChild("MenuPane")->isVisible()) getChild("MenuPane.Menu")->focus();
		else focus();
	}

	// The menu opened from the level, by hand or by Ctrl+O. Return in its
	// name box saves only once Save has asked for the name there: the box
	// names a level to load as often as one to save under, and a name typed
	// to load must not overwrite that level.
	void openMenu()
	{
		static_cast<GUI_EditBox*>(getChild("MenuPane.Menu.Filename"))->setSubmitButton(0);
		getChild("MenuPane.Menu")->focus();
	}

	void updateToolTips()
	{
		for(int x = 0; x < 24; x++)
		{
			for(int y = 0; y < 3; y++)
			{
				Object* p_obj = editor.p_currentCat->getFrontObjectAt(Vec2i(x, y));
				if(p_obj) p_toolTip[x][y]->setToolTip(p_obj->getToolTip());
				else p_toolTip[x][y]->setToolTip("");
			}
		}
	}

private:
	// The pin at a point of the level. The parts in the cells around it are
	// asked too: a pin sits on its part's edge and is found up to two pixels
	// off it.
	Pin* pinAt(const Vec2i& position)
	{
		const Vec2i p = position / 16;
		Pin* p_found = 0;
		for(int x = p.x - 1; x <= p.x + 1; x++)
		{
			for(int y = p.y - 1; y <= p.y + 1; y++)
			{
				std::vector<Object*> objects = editor.p_level->getObjectsAt(Vec2i(x, y));
				for(std::vector<Object*>::const_iterator i = objects.begin(); i != objects.end(); ++i)
				{
					if((*i)->getFlags() & Object::OF_ELECTRONICS)
					{
						Electronics* e = static_cast<Electronics*>(*i);
						Pin* p_pin = e->getPinAt(position - 16 * e->getPosition());
						if(p_pin) p_found = p_pin;
					}
				}
			}
		}
		return p_found;
	}

	GS_LevelEditor& editor;
	Vec2i oldCursor;
	bool confirmed;
	GUI_Element* p_clickWhenConfirmed;
	// The file list was opened to load, by Ctrl+O or a Load with no name,
	// and a level picked there loads.
	bool searchLoads;
	bool realDown;
	GUI_Element* p_toolTip[24][3];
	Options* p_options;
	Help* p_help;
};

GS_LevelEditor::GS_LevelEditor() : GameState("GS_LevelEditor"), engine(Engine::inst()), pipetteObjectAttributes("")
{
	p_level = 0;
	p_currentCat = 0;
	p_clipboard = 0;
	p_teleporter = 0;
	p_hint = 0;
	p_currentPin = 0;
	p_startPin = 0;
	p_changeBefore = 0;
}

GS_LevelEditor::~GS_LevelEditor()
{
}

namespace
{
	// The language of the text around the caret: that of the last "\xA7xx:"
	// marker ahead of it, "" for the active language ahead of the first. A
	// caret right before a marker still stands in the section before it. The
	// sign is the byte languages.txt carries, hence the escape.
	std::string languageAtCursor(const std::string& text, uint cursor)
	{
		std::string::size_type at = cursor;
		while(at > 0)
		{
			at = text.rfind('\xA7', at - 1);
			if(at == std::string::npos) break;

			// A marker is the sign, one to eight letters and a colon. Anything
			// else with the sign in it is text, and the search goes on behind it.
			std::string::size_type end = at + 1;
			while(end < text.length() && end - at <= 8 && isalpha(static_cast<unsigned char>(text[end]))) end++;
			if(end > at + 1 && end < text.length() && text[end] == ':') return text.substr(at + 1, end - at - 1);
		}
		return "";
	}

	// The red frame around a tile, lighter at the top left. The bottom edge
	// reaches a pixel past d, or the bottom left corner is lit by neither edge
	// (Renderer::hairlineRect says why). That edge because it is one colour:
	// lengthening the left one would stretch its ramp and shift every step.
	void highlightTile(Renderer& renderer, const Vec2i& p)
	{
		const Vec4f light(1.0f, 0.75f, 0.75f, 1.0f), dark(1.0f, 0.15f, 0.15f, 1.0f);
		const Vec2f a = static_cast<Vec2f>(p);
		const Vec2f b = a + Vec2f(16.0f, 0.0f), c = a + Vec2f(16.0f, 16.0f), d = a + Vec2f(0.0f, 16.0f);
		renderer.hairline(a, b, light, dark);
		renderer.hairline(b, c, dark);
		renderer.hairline(c, d - Vec2f(1.0f, 0.0f), dark);
		renderer.hairline(d, a, dark, light);
	}

	// The marching ants around a selection: a black loop a pixel down and
	// right, a white one over it, both two pixels wide and dashed four on,
	// four off, the dashes moving one pixel every 40 ms.
	void marchingAnts(Renderer& renderer, const Vec2i& p1, const Vec2i& p2, uint time)
	{
		const float phase = 4.0f - static_cast<float>((time / 40) % 8);
		std::vector<Vec2f> loop;
		const Vec2f lo = static_cast<Vec2f>(p1 + Vec2i(1, 1)), hi = static_cast<Vec2f>(p2 + Vec2i(1, 1));
		loop.push_back(lo);
		loop.push_back(Vec2f(hi.x, lo.y));
		loop.push_back(hi);
		loop.push_back(Vec2f(lo.x, hi.y));
		renderer.dashes(loop, 2.0f, Vec4f(0.0f, 0.0f, 0.0f, 0.75f), 4.0f, 4.0f, phase, true);
		for(size_t i = 0; i < loop.size(); i++) loop[i] -= Vec2f(1.0f, 1.0f);
		renderer.dashes(loop, 2.0f, Vec4f(1.0f, 1.0f, 1.0f, 0.75f), 4.0f, 4.0f, phase, true);
	}
}

void GS_LevelEditor::onRender()
{
	Renderer& renderer = Renderer::inst();

	// render the level
	p_level->render();

	Vec2i p = engine.getCursorPosition() / 16;
	if(p.x < Level::WIDTH && p.y < Level::HEIGHT)
	{
		// highlight the current tile
		highlightTile(renderer, p * 16);
	}

	if(currentMode == 6)
	{
		// highlight the selected pin and the start pin: frames through the
		// pixel centres
		renderer.push();
		renderer.translate(0.5f, 0.5f);

		if(p_currentPin)
		{
			const Vec2i p = p_currentPin->getScreenPosition();
			renderer.hairlineRect(static_cast<Vec2f>(p - Vec2i(2, 2)), static_cast<Vec2f>(p + Vec2i(2, 2)), Vec4f(0.0f, 0.0f, 1.0f, 0.75f));
		}

		if(p_startPin)
		{
			const Vec2i p = p_startPin->getScreenPosition();
			renderer.hairlineRect(static_cast<Vec2f>(p - Vec2i(2, 2)), static_cast<Vec2f>(p + Vec2i(2, 2)), Vec4f(0.5f, 0.5f, 1.0f, 1.0f));
		}

		renderer.pop();
	}

	// render the background gradient
	{
		const Vec2f corners[4] = {Vec2f(0.0f, 400.0f), Vec2f(640.0f, 400.0f), Vec2f(640.0f, 480.0f), Vec2f(0.0f, 480.0f)};
		const Vec4f top(0.15f, 0.2f, 0.2f, 1.0f), bottom(0.4f, 0.51f, 0.25f, 1.0f);
		const Vec4f colors[4] = {top, top, bottom, bottom};
		renderer.quad(corners, colors);
	}

	if(rectStart.x != -1)
	{
		Vec2i pMin(min(rectStart.x, rectEnd.x), min(rectStart.y, rectEnd.y));
		Vec2i pMax(max(rectStart.x, rectEnd.x), max(rectStart.y, rectEnd.y));
		Vec2i p1 = pMin * 16;
		Vec2i p2 = pMax * 16 + Vec2i(16, 16);

		if(currentMode == 2)
		{
			// render the rectangle, blue for the left button and red for
			// the right
			const bool blue = (drawStartButtons & 1) != 0;
			const Vec4f fill = blue ? Vec4f(0.25f, 0.25f, 1.0f, 0.4f) : Vec4f(1.0f, 0.25f, 0.25f, 0.4f);
			const Vec4f frame = blue ? Vec4f(0.25f, 0.25f, 1.0f, 0.85f) : Vec4f(1.0f, 0.25f, 0.25f, 0.85f);
			const Vec2f from = static_cast<Vec2f>(p1);
			const Vec2f to = static_cast<Vec2f>(p2);
			renderer.rect(from, to, fill);
			renderer.hairlineRect(from, to, frame);
		}
		else if(currentMode == 4)
		{
			// render the rectangle
			marchingAnts(renderer, p1, p2, engine.getTime());
		}
	}

	// render the selected category
	renderer.push();
	renderer.translate(245.0f, 428.0f);
	p_currentCat->render();

	// render the selection
	if(currentCat == currentBrush.z && !pipetteUsed)
	{
		highlightTile(renderer, Vec2i(currentBrush.x * 16, currentBrush.y * 16));
	}

	renderer.pop();

	if(p_hint)
	{
		GUI_MultiLineEditBox* p_box = static_cast<GUI_MultiLineEditBox*>(gui["LevelEditor.EditHintPane.EditHint.Text"]);
		p_hint->setText(p_box->getText());
		p_hint->setPreviewLanguage(languageAtCursor(p_box->getText(), p_box->getCursor()));
		p_hint->render(RL_HINT_PREVIEW, Vec2i(0, 0), Vec4f(1.0f));
	}
}

void GS_LevelEditor::onUpdate()
{
	// remove the old objects, add the new ones
	p_level->removeOldObjects();
	p_level->addNewObjects();
}

void GS_LevelEditor::onEnter(const ParameterBlock& context)
{
	// load an empty level
	p_level = new Level;
	p_level->setInEditor(true);
	p_level->load("level_default.xml");

	// load the categories
	for(int i = 0; i < 5; i++)
	{
		p_cat[i] = new Level;
		p_cat[i]->setInEditor(true);
		p_cat[i]->setInCat(true);
		char filename[256] = "";
		sprintf(filename, "cat%d.xml", i);
		p_cat[i]->load(filename);
		p_cat[i]->setElectricityOn(true);
	}

	currentLayer = 1;
	currentMode = 0;
	currentCat = 0;
	p_currentCat = p_cat[0];
	currentBrush = Vec3i(0, 0, 0);
	pipetteObjectType = "";
	pipetteTileID = 0;
	pipetteUsed = false;
	oldMode = -1;
	rectStart = rectEnd = Vec2i(-1, -1);
	drawStartButtons = 0;
	transitionGround[0] = transitionGround[1] = -1;
	p_changeBefore = 0;
	clipboardSize = Vec2i(0, 0);
	p_clipboard = 0;

	p_teleporter = 0;
	p_hint = 0;
	p_currentPin = 0;
	p_startPin = 0;

	originalFilename = "";
	setSavePoint();

	// create the dialog
	new LevelEditorGUI(*this);
}

void GS_LevelEditor::onLeave(const ParameterBlock& context)
{
	clearUndo();
	clearRedo();

	// free the resources
	delete[] p_clipboard;
	clipboardSize = Vec2i(0, 0);
	p_clipboard = 0;
	delete p_level;
	p_level = 0;
	for(int i = 0; i < 5; i++)
	{
		delete p_cat[i];
		p_cat[i] = 0;
	}

	// delete the dialog
	delete gui["LevelEditor"];
}

void GS_LevelEditor::onGetFocus()
{
	engine.playMusic("menu.ogg", 0.0f, true);

	gui["LevelEditor"]->focus();
}

void GS_LevelEditor::onLoseFocus()
{
	gui["LevelEditor"]->hide();
}

// The stroke ends here. Nothing promises a release for a button held as the
// focus went, and the next press would otherwise count as the other button
// cancelling a stroke that is long finished, or aim a teleporter. A change a
// dialog holds stays open for its OK or Cancel.
void GS_LevelEditor::onAppLoseFocus()
{
	// No release comes for a button held now, so the stroke ends here: its
	// rectangle, the teleporter it was aiming and, unless a dialog holds them,
	// its change and the mode it had borrowed.
	drawStartButtons = 0;
	p_teleporter = 0;
	if(currentMode == 2) rectStart = rectEnd = Vec2i(-1, -1);
	if(!gui["LevelEditor.EditHintPane"]->isVisible() && !gui["LevelEditor.SettingsPane"]->isVisible())
	{
		endChange();
		if(oldMode != -1)
		{
			setMode(oldMode);
			oldMode = -1;
		}
	}
}

void GS_LevelEditor::beginChange()
{
	endChange();
	p_changeBefore = p_level->save();
}

void GS_LevelEditor::endChange()
{
	if(!p_changeBefore) return;
	TiXmlDocument* p_before = p_changeBefore;
	p_changeBefore = 0;
	if(levelDiffers(p_before)) pushUndoPoint(p_before);
	else delete p_before;
}

void GS_LevelEditor::cancelChange()
{
	if(!p_changeBefore) return;
	Level* p_before = new Level;
	p_before->setInEditor(true);
	p_before->load(p_changeBefore);
	delete p_changeBefore;
	p_changeBefore = 0;
	replaceLevel(p_before);
}

// By the XML a save writes, which is what an undo point holds and what
// wasChanged() compares too: save() sorts the objects first, so the same
// level always reads the same.
bool GS_LevelEditor::levelDiffers(const TiXmlDocument* p_doc)
{
	TiXmlDocument* p_now = p_level->save();
	std::string before, now;
	before << *p_doc;
	now << *p_now;
	delete p_now;
	return before != now;
}

uint GS_LevelEditor::getUndoDepth() const
{
	return static_cast<uint>(undoList.size());
}

uint GS_LevelEditor::getRedoDepth() const
{
	return static_cast<uint>(redoList.size());
}

void GS_LevelEditor::pushUndoPoint(TiXmlDocument* p_doc)
{
	clearRedo();
	undoList.push_front(p_doc);

	// cap at 64 steps
	while(undoList.size() > 64)
	{
		delete undoList.back();
		undoList.erase(--undoList.end());
	}
}

void GS_LevelEditor::undo()
{
	// A stroke still under way is a step of its own, undone first.
	endChange();

	if(!undoList.empty())
	{
		redoList.push_front(p_level->save());
		TiXmlDocument* p_old = undoList.front();
		undoList.erase(undoList.begin());

		Level* p_undone = new Level;
		p_undone->setInEditor(true);
		p_undone->load(p_old);

		delete p_old;
		replaceLevel(p_undone);
	}
}

void GS_LevelEditor::redo()
{
	// One under way is a new step, and there is nothing left to redo.
	endChange();

	if(!redoList.empty())
	{
		undoList.push_front(p_level->save());
		TiXmlDocument* p_old = redoList.front();
		redoList.erase(redoList.begin());

		Level* p_redone = new Level;
		p_redone->setInEditor(true);
		p_redone->load(p_old);

		delete p_old;
		replaceLevel(p_redone);
	}
}

void GS_LevelEditor::replaceLevel(Level* p_newLevel)
{
	delete p_level;
	p_level = p_newLevel;
	forgetPickedObjects();
}

// The two pins, the teleporter being aimed and the note being written belong
// to the level's objects, and whatever deletes those has to drop them: a wire
// started and then undone would otherwise be drawn from, and connected to, a
// freed pin.
void GS_LevelEditor::forgetPickedObjects()
{
	p_teleporter = 0;
	p_hint = 0;
	p_currentPin = 0;
	p_startPin = 0;
}

void GS_LevelEditor::clearUndo()
{
	delete p_changeBefore;
	p_changeBefore = 0;

	while(!undoList.empty())
	{
		delete undoList.front();
		undoList.erase(undoList.begin());
	}
}

void GS_LevelEditor::clearRedo()
{
	while(!redoList.empty())
	{
		delete redoList.front();
		redoList.erase(redoList.begin());
	}
}

bool GS_LevelEditor::wasChanged()
{
	TiXmlDocument* p_doc = p_level->save();
	std::string currentXML;
	currentXML << *p_doc;
	delete p_doc;
	return currentXML != lastSavedXML;
}

void GS_LevelEditor::setSavePoint()
{
	TiXmlDocument* p_doc = p_level->save();
	lastSavedXML = "";
	lastSavedXML << *p_doc;
	delete p_doc;
}

void GS_LevelEditor::setMode(int mode,
							 bool updateRadioButtons)
{
	if(mode != currentMode)
	{
		p_currentPin = p_startPin = 0;
	}

	currentMode = mode;
	rectStart = rectEnd = Vec2i(-1, -1);

	if(updateRadioButtons)
	{
		char name[256] = "";
		sprintf(name, "LevelEditor.Mode%d", mode);
		static_cast<GUI_RadioButton*>(gui[name])->check();
	}
}

void GS_LevelEditor::setCat(int cat)
{
	currentCat = cat;
	p_currentCat = p_cat[cat];
	static_cast<LevelEditorGUI*>(gui["LevelEditor"])->updateToolTips();
}

void GS_LevelEditor::draw(const Vec2i& where,
						  bool shift)
{
	Level* p_brushCat = p_cat[currentBrush.z];
	Vec2i brush(currentBrush.x, currentBrush.y);

	if((p_brushCat == p_cat[0] && !pipetteUsed) || (pipetteUsed && pipetteObjectType.empty()))
	{
		uint tile;

		if(pipetteUsed) tile = pipetteTileID;
		else
		{
			// A tile is selected. Which one?
			uint l0 = p_brushCat->getTileAt(0, brush);
			uint l1 = p_brushCat->getTileAt(1, brush);
			tile = l0 + l1;
		}

		// put this tile into the cell on the chosen layer
		p_level->setTileAt(currentLayer, where, tile);
	}
	else
	{
		std::string objectType;
		TiXmlElement objectAttributes("");

		if(pipetteUsed)
		{
			objectType = pipetteObjectType;
			objectAttributes = pipetteObjectAttributes;
		}
		else
		{
			// An object is selected. Which one?
			Object* p_obj = p_brushCat->getFrontObjectAt(brush);
			if(p_obj)
			{
				objectType = p_obj->getType();
				p_obj->saveAttributes(&objectAttributes);
			}
		}

		if(!objectType.empty())
		{
			if(objectType != "Rail" && objectType != "Hint" && !shift)
			{
				// Never delete rails here!
				p_level->clearPosition(where, "Rail");
				p_level->removeOldObjects();
			}
			else if(objectType == "Rail" && !shift)
			{
				// Never delete lava here
				p_level->clearPosition(where, "Lava");
				p_level->removeOldObjects();
			}
			else if(objectType == "Hint")
			{
				// Is there already a note here?
				Object* p_oldObj = p_level->getFrontObjectAt(where);
				if(p_oldObj)
				{
					if(p_oldObj->getType() == "Hint")
					{
						if(currentMode == 0)
						{
							// Probably a slip. Instead of deleting the note, go into modify mode.
							oldMode = currentMode;
							setMode(1);
							modify(where, 1, false, true);
							return;
						}
						else
						{
							// do nothing
							return;
						}
					}
				}

				// No note there. Clear the cell except for rails, and with
				// shift held nothing at all, as in the two branches above.
				if(!shift)
				{
					p_level->clearPosition(where, "Rail");
					p_level->removeOldObjects();
				}
			}

			// put the object into the chosen cell
			p_level->getPresets()->instancePreset(objectType, where, &objectAttributes);

			// "make it stick"
			p_level->removeOldObjects();
			p_level->addNewObjects();

			if(currentMode == 0)
			{
				bool isTeleporter = objectType == "Teleporter";
				bool isHint = objectType == "Hint";
				if(isTeleporter || isHint)
				{
					// The placement is a step of its own, and aiming the
					// teleporter or writing the note the next change.
					beginChange();

					// go into modify mode at once
					oldMode = currentMode;
					setMode(1);
					modify(where, 1, false, true);
				}
			}
		}
	}
}

void GS_LevelEditor::erase(const Vec2i& where,
						   bool shift)
{
	// Is there an object here?
	Object* p_obj = shift ? 0 : p_level->getFrontObjectAt(where);
	if(p_obj) p_level->removeObject(p_obj);
	else
	{
		// clear the tile on the current layer
		p_level->setTileAt(currentLayer, where, 0);
	}

	p_level->removeOldObjects();
}

void GS_LevelEditor::clear(const Vec2i& where,
						   bool allLayers)
{
	// delete the objects
	p_level->clearPosition(where);

	if(allLayers)
	{
		for(int layer = 0; layer < 2; layer++) p_level->setTileAt(layer, where, 0);
	}
	else
	{
		// clear the tile on the current layer
		p_level->setTileAt(currentLayer, where, 0);
	}

	p_level->removeOldObjects();
}

void GS_LevelEditor::modify(const Vec2i& where,
							int buttons,
							bool shift,
							bool press)
{
	// Is there an object here?
	Object* p_obj = p_level->getFrontObjectAt(where);
	if(!p_obj) return;

	const std::string& type = p_obj->getType();

	// Aimed and written from a press only: a drag that crosses one is on its
	// way somewhere else.
	if(!press && (type == "Teleporter" || type == "Hint")) return;

	if(type == "Teleporter")
	{
		p_teleporter = static_cast<Teleporter*>(p_obj);
	}
	else if(type == "Hint")
	{
		p_hint = static_cast<Hint*>(p_obj);
		drawStartButtons = 0;
		gui["LevelEditor.EditHintPane.EditHint.Text"]->focus();
		static_cast<GUI_MultiLineEditBox*>(gui["LevelEditor.EditHintPane.EditHint.Text"])->setText(p_hint->getText());
	}
	else p_obj->changeInEditor(shift ? 1 : 0);
}

namespace
{
	// Lawn and sand and the twelve pieces between them, as the transition tool
	// knows them: by the corners they show sand in, 1 the top left, 2 the top
	// right, 4 the bottom left and 8 the bottom right. No piece has sand in
	// two opposite corners alone, 6 and 9.
	const char GROUND_BY_SAND[16] = {'.', '8', '6', '7', '2', '5', 0, 'a', '0', 0, '3', 'b', '1', 'c', 'd', '4'};

	// The cave wall's pieces by the sides it ends on, 1 the top, 2 the right,
	// 4 the bottom and 8 the left. None ends on two opposite sides: the pieces
	// draw no wall narrower than two cells.
	const char CAVE_BY_OPEN[16] = {'V', 'Q', 'W', 'R', 'i', 0, 'j', 0, 'U', 'P', 0, 0, 'Z', 0, 0, 0};

	// And its inner corners, for a cell walled in on all four sides, by the
	// diagonal that is open, in the corners' order above.
	const char CAVE_NOTCH[4] = {'o', 'Y', 'T', 'O'};
	const char CAVE_PIECES[] = "PQRUVWZijOTYo";

	// The sand corners of a ground tile, -1 for any other.
	int sandCorners(uint tile)
	{
		for(int sand = 0; sand < 16; sand++)
		{
			if(GROUND_BY_SAND[sand] && tile == static_cast<uint>(GROUND_BY_SAND[sand])) return sand;
		}
		return -1;
	}

	bool isCavePiece(uint tile)
	{
		for(const char* p = CAVE_PIECES; *p; p++)
		{
			if(tile == static_cast<uint>(*p)) return true;
		}
		return false;
	}

	// A piece only where the level's skin has got it, 0 otherwise - space has
	// no o - since one it has not got draws nothing and lets the player walk
	// through.
	uint inSkin(Level& level, char piece)
	{
		return piece && level.getTileSet()->getTileInfo(piece).type != -1 ? static_cast<uint>(piece) : 0;
	}

	// Whether the lawn and sand cells around a corner of the grid show both
	// there: a hard edge. A corner of the grid is named by the cell whose top
	// left corner it is, so the cell point - Vec2i(i & 1, i >> 1) has it as
	// its corner i, and a cell's corner c is cell + Vec2i(c & 1, c >> 1).
	bool isHardEdge(Level& level, int layer, const Vec2i& point)
	{
		int shown = 0;
		for(int i = 0; i < 4; i++)
		{
			const int sand = sandCorners(level.getTileAt(layer, point - Vec2i(i & 1, i >> 1)));
			if(sand >= 0) shown |= (sand >> i & 1) ? 2 : 1;
		}
		return shown == 3;
	}

	// Gives a corner of the grid to one ground, and the four cells around it
	// along, so that they meet there without a seam. A cell left with sand in
	// two opposite corners and lawn in the other two, which no piece shows,
	// gives one of its two corners of the other ground along too, where that
	// is a hard edge as well. False, and the level as it was, where that does
	// not help.
	bool settle(Level& level, int layer, const Vec2i& point, bool toSand)
	{
		const int MAX_POINTS = 8;
		Vec2i points[MAX_POINTS];
		points[0] = point;
		int numPoints = 1;
		for(;;)
		{
			Vec2i cells[4 * MAX_POINTS];
			int sand[4 * MAX_POINTS];
			int numCells = 0;
			for(int p = 0; p < numPoints; p++)
			{
				for(int i = 0; i < 4; i++)
				{
					const Vec2i cell = points[p] - Vec2i(i & 1, i >> 1);
					int c = 0;
					while(c < numCells && cells[c] != cell) c++;
					if(c == numCells)
					{
						sand[c] = sandCorners(level.getTileAt(layer, cell));
						if(sand[c] < 0) continue;
						cells[numCells++] = cell;
					}
					sand[c] = toSand ? sand[c] | 1 << i : sand[c] & ~(1 << i);
				}
			}

			int odd = 0;
			while(odd < numCells && inSkin(level, GROUND_BY_SAND[sand[odd]])) odd++;
			if(odd == numCells)
			{
				for(int c = 0; c < numCells; c++) level.setTileAt(layer, cells[c], static_cast<uint>(GROUND_BY_SAND[sand[c]]));
				return true;
			}
			if(numPoints == MAX_POINTS) return false;

			int corner = 0;
			while(corner < 4 && (((sand[odd] >> corner & 1) != 0) == toSand || !isHardEdge(level, layer, cells[odd] + Vec2i(corner & 1, corner >> 1)))) corner++;
			if(corner == 4) return false;
			points[numPoints++] = cells[odd] + Vec2i(corner & 1, corner >> 1);
		}
	}

	// Smooths the hard edges at the four corners of a cell: each goes to the
	// ground winning the stroke, or to the other where the winner cannot have
	// it. A corner the cells around it agree on stays as it is, so a second
	// stroke changes nothing.
	void blendGround(Level& level, int layer, const Vec2i& where, bool sandWins)
	{
		for(int corner = 0; corner < 4; corner++)
		{
			const Vec2i point = where + Vec2i(corner & 1, corner >> 1);
			if(isHardEdge(level, layer, point) && !settle(level, layer, point, sandWins)) settle(level, layer, point, !sandWins);
		}
	}

	// Whatever is solid joins a cave wall seamlessly, a brick wall as much as
	// the rock, and so does the edge of the level: none of them gets an edge.
	bool isWall(Level& level, int layer, const Vec2i& where)
	{
		if(!level.isValidPosition(where)) return true;
		const int type = level.getTileSet()->getTileInfo(level.getTileAt(layer, where)).type;
		return type == 1 || type == 2;
	}

	// The piece a cave wall cell gets, 0 for any other cell and where it stays
	// as it is: by the sides the wall ends on, and inside it by the open
	// diagonal. A cell with two open diagonals can show only one of them, and
	// keeps the one it has, if it has one, so that a corner chosen by hand
	// stays.
	uint shapeCaveWall(Level& level, int layer, const Vec2i& where)
	{
		const uint tile = level.getTileAt(layer, where);
		if(!isCavePiece(tile)) return 0;

		const Vec2i sides[4] = {Vec2i(0, -1), Vec2i(1, 0), Vec2i(0, 1), Vec2i(-1, 0)};
		int open = 0;
		for(int i = 0; i < 4; i++)
		{
			if(!isWall(level, layer, where + sides[i])) open |= 1 << i;
		}
		if(open) return inSkin(level, CAVE_BY_OPEN[open]);

		int notches = 0;
		for(int corner = 0; corner < 4; corner++)
		{
			if(!isWall(level, layer, where + Vec2i((corner & 1) ? 1 : -1, (corner & 2) ? 1 : -1))) notches |= 1 << corner;
		}
		for(int corner = 0; corner < 4; corner++)
		{
			const uint notch = static_cast<uint>(CAVE_NOTCH[corner]);
			if(notches == 1 << corner || ((notches >> corner & 1) && tile == notch)) return inSkin(level, CAVE_NOTCH[corner]);
		}
		return 'V';
	}
}

// Both layers, whatever the one chosen: the ground lies on layer 0 and the
// cave wall on layer 1, and the tool is for both.
void GS_LevelEditor::transition(const Vec2i& position,
								bool press)
{
	const Vec2i where = position / 16;

	// The ground a stroke starts on reaches half a cell over each hard edge
	// the stroke passes, whichever side of it the cursor runs along: started
	// on a sand area the stroke rounds it off outwards, started beside it,
	// inwards. So a path or a single cell drawn with the pen and traced over
	// grows soft edges rather than being swallowed. On a piece, the ground
	// under the cursor is what its nearest corner shows.
	const int corner = (position.x % 16 >= 8 ? 1 : 0) | (position.y % 16 >= 8 ? 2 : 0);
	for(int layer = 0; layer < Level::NUM_LAYERS; layer++)
	{
		const int sand = sandCorners(p_level->getTileAt(layer, where));
		if(press) transitionGround[layer] = -1;
		if(transitionGround[layer] < 0 && sand >= 0) transitionGround[layer] = sand >> corner & 1;
		if(transitionGround[layer] >= 0) blendGround(*p_level, layer, where, transitionGround[layer] == 1);

		// A wall's shape follows from where the walls are and nothing else, so
		// the cells around the cursor are shaped too: a stroke beside a wall
		// reaches it.
		for(int y = -1; y <= 1; y++)
		{
			for(int x = -1; x <= 1; x++)
			{
				const uint tile = shapeCaveWall(*p_level, layer, where + Vec2i(x, y));
				if(tile) p_level->setTileAt(layer, where + Vec2i(x, y), tile);
			}
		}
	}
}

bool GS_LevelEditor::copy()
{
	if(currentMode != 4 || rectStart.x < 0) return false;

	// free the old clipboard
	delete[] p_clipboard;

	// make room for the new clipboard
	Vec2i pMin(min(rectStart.x, rectEnd.x), min(rectStart.y, rectEnd.y));
	Vec2i pMax(max(rectStart.x, rectEnd.x), max(rectStart.y, rectEnd.y));
	clipboardSize = Vec2i(1, 1) + pMax - pMin;
	p_clipboard = new FieldInClipboard[clipboardSize.x * clipboardSize.y];

	// copy into it
	uint index = 0;
	for(int x = pMin.x; x <= pMax.x; x++)
	{
		for(int y = pMin.y; y <= pMax.y; y++)
		{
			Vec2i p(x, y);

			// copy the tiles
			for(int layer = 0; layer < 2; layer++) p_clipboard[index].tile[layer] = p_level->getTileAt(layer, p);

			// copy the objects
			const std::vector<Object*>& objects = p_level->getObjectsAt(p);
			for(std::vector<Object*>::const_iterator i = objects.begin(); i != objects.end(); ++i)
			{
				TiXmlElement attributes("");
				(*i)->saveAttributes(&attributes);
				p_clipboard[index].objectTypes.push_back((*i)->getType());
				p_clipboard[index].objectAttributes.push_back(attributes);
			}

			index++;
		}
	}

	return true;
}

bool GS_LevelEditor::paste(const Vec2i& where)
{
	if(!p_clipboard || !clipboardSize.x || !clipboardSize.y) return false;

	// The whole rectangle has to fit: a corner outside the level would put
	// tiles and objects where no cell is.
	if(!p_level->isValidPosition(where) || !p_level->isValidPosition(where + clipboardSize + Vec2i(-1, -1))) return false;

	uint index = 0;
	for(int x = 0; x < clipboardSize.x; x++)
	{
		for(int y = 0; y < clipboardSize.y; y++)
		{
			Vec2i p = where + Vec2i(x, y);

			// paste the tiles
			for(int layer = 0; layer < 2; layer++)
			{
				uint tile = p_clipboard[index].tile[layer];
				if(tile) p_level->setTileAt(layer, p, tile);
			}

			if(!p_clipboard[index].objectTypes.empty())
			{
				// delete the old objects
				p_level->clearPosition(p);
				p_level->removeOldObjects();

				// paste the objects
				std::list<std::string>::const_iterator i;
				std::list<TiXmlElement>::iterator j;
				for(i = p_clipboard[index].objectTypes.begin(), j = p_clipboard[index].objectAttributes.begin();
					i != p_clipboard[index].objectTypes.end() && j != p_clipboard[index].objectAttributes.end();
					i++, j++)
				{
					p_level->getPresets()->instancePreset(*i, p, &(*j));
					p_level->addNewObjects();
				}
			}

			index++;
		}
	}

	rectStart = where;
	rectEnd = where + clipboardSize + Vec2i(-1, -1);

	rectStart.x = clamp(rectStart.x, 0, Level::WIDTH - 1);
	rectStart.y = clamp(rectStart.y, 0, Level::HEIGHT - 1);
	rectEnd.x = clamp(rectEnd.x, 0, Level::WIDTH - 1);
	rectEnd.y = clamp(rectEnd.y, 0, Level::HEIGHT - 1);

	return true;
}

bool GS_LevelEditor::clear()
{
	if(currentMode != 4 || rectStart.x < 0) return false;

	Vec2i pMin(min(rectStart.x, rectEnd.x), min(rectStart.y, rectEnd.y));
	Vec2i pMax(max(rectStart.x, rectEnd.x), max(rectStart.y, rectEnd.y));
	for(int x = pMin.x; x <= pMax.x; x++)
	{
		for(int y = pMin.y; y <= pMax.y; y++)
		{
			clear(Vec2i(x, y), true);
		}
	}

	return true;
}