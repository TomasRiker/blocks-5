#include "pch.h"
#include "gs_leveleditor.h"
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

class LevelEditorGUI : public GUI_Element, public sigslot::has_slots<>
{
public:
	LevelEditorGUI(GS_LevelEditor& editor) : GUI_Element("LevelEditor", 0, Vec2i(0, 0), Vec2i(640, 480)), editor(editor)
	{
		load("leveleditor.xml");

		// Tool-Tip-Elemente erstellen
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
		static_cast<GUI_Button*>(getChild("MenuPane.Menu.Help"))->connectClicked(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_Button*>(getChild("MenuPane.Menu.OK"))->connectClicked(this, &LevelEditorGUI::handleClick);
		static_cast<GUI_Button*>(getChild("MenuPane.Menu.Quit"))->connectClicked(this, &LevelEditorGUI::handleClick);
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
		realDown = true;

		p_help = new Help(this);
	}

	~LevelEditorGUI()
	{
		delete p_help;
	}

	void onUpdate()
	{
		char s[256];

		sprintf(s, "%s: %d", localizeString("$LE_LAYER").c_str(), editor.currentLayer);
		static_cast<GUI_Button*>(getChild("Layer"))->setTitle(s);

		sprintf(s, "%s: %d", localizeString("$LE_DIAMONDS").c_str(), editor.p_level->getNumDiamondsNeeded());
		static_cast<GUI_StaticText*>(getChild("NumDiamondsNeeded"))->setText(s);

		static_cast<GUI_CheckBox*>(getChild("ElectricityOn"))->check(editor.p_level->isElectricityOn());
	}

	void onMouseDown(const Vec2i& position,
					 int buttons)
	{
		bool shift = editor.engine.isKeyDown(SDLK_LSHIFT) || editor.engine.isKeyDown(SDLK_RSHIFT);

		if(position.y < 400)
		{
			Vec2i p = position / 16;

			if(editor.currentMode == 0)
			{
				if(realDown)
				{
					// Ist das ein Abbruchversuch?
					if(editor.drawStartButtons && editor.drawStartButtons != buttons)
					{
						// Ja.
						editor.undo();
						editor.drawStartButtons = 0;
						buttons = 0;
					}
					else
					{
						editor.drawStartButtons = buttons;
					}
				}

				if(buttons & 1)
				{
					// Zeichenstift anwenden
					if(realDown) editor.createUndoPoint();
					editor.draw(p, shift);
				}
				else if(buttons & 3)
				{
					// Radiergummi
					if(realDown) editor.createUndoPoint();
					editor.erase(p, shift);
				}
			}
			else if(editor.currentMode == 1)
			{
				if(buttons & 1)
				{
					if(realDown) editor.createUndoPoint();

					if(editor.p_teleporter)
					{
						// Zielpunkt des Teleporters setzen
						editor.p_teleporter->setTargetPosition(p);
					}
					else
					{
						// modifizieren
						if(!editor.modify(p, buttons, shift)) editor.deleteLastUndoPoint();
					}
				}
			}
			else if(editor.currentMode == 2 || editor.currentMode == 4)
			{
				if(realDown)
				{
					// Ist das ein Abbruchversuch?
					if(editor.drawStartButtons && editor.drawStartButtons != buttons)
					{
						// Ja.
						editor.rectStart = editor.rectEnd = Vec2i(-1, -1);
						editor.drawStartButtons = 0;
					}
					else
					{
						// Startpunkt für das Rechteck setzen und merken, welche Maustaste gedrückt wurde
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
					// Ist das ein Abbruchversuch?
					if(editor.drawStartButtons && editor.drawStartButtons != buttons)
					{
						// Ja.
						editor.undo();
						editor.drawStartButtons = 0;
						buttons = 0;
					}
					else
					{
						editor.drawStartButtons = buttons;
					}
				}

				if(buttons)
				{
					// Übergang erzeugen
					if(realDown) editor.createUndoPoint();
					editor.transition(p);
				}
			}
			else if(editor.currentMode == 5)
			{
				if(buttons & 1)
				{
					editor.pipetteTileID = 0;
					editor.pipetteObjectType = "";
					editor.pipetteUsed = true;

					// Ist da ein Objekt?
					Object* p_object = editor.p_level->getFrontObjectAt(p);
					if(p_object)
					{
						// Typ und Attribute merken
						editor.pipetteObjectType = p_object->getType();
						editor.pipetteObjectAttributes = TiXmlElement("");
						p_object->saveAttributes(&editor.pipetteObjectAttributes);
					}
					else
					{
						// Welches Tile ist da auf dem aktuellen Layer?
						editor.pipetteTileID = editor.p_level->getTileAt(editor.currentLayer, p);
					}
				}
			}
			else if(editor.currentMode == 6)
			{
				if(editor.p_currentPin)
				{
					if(buttons & 1)
					{
						if(editor.p_startPin)
						{
							// verbinden
							editor.createUndoPoint();
							bool r = Pin::connect(editor.p_startPin, editor.p_currentPin);
							if(!r)
							{
								// Fehlermeldung anzeigen
								editor.messageText = "$LE_ERROR_INVALID_CONNECTION";
								editor.messageCounter = 200;
								editor.messageType = 1;

								editor.deleteLastUndoPoint();
							}

							editor.p_currentPin = editor.p_startPin = 0;
						}
						else editor.p_startPin = editor.p_currentPin;
					}
					else if(buttons & 3)
					{
						if(editor.p_currentPin->isConnected())
						{
							// alle Verbindungen dieses Pins lösen
							editor.createUndoPoint();
							editor.p_currentPin->disconnectAll();
							editor.p_currentPin = editor.p_startPin = 0;
						}
					}
				}
				else
				{
					if(buttons & 3)
					{
						editor.p_currentPin = editor.p_startPin = 0;
					}
				}
			}
		}
		else if(position.y >= 428 && position.x >= 245)
		{
			// Es wurde wahrscheinlich ein neues Tile/Objekt ausgewählt.
			Vec2i p = (position - Vec2i(245, 428)) / 16;

			// Ist da überhaupt etwas in der aktuellen Kategorie?
			uint l0 = editor.p_currentCat->getTileAt(0, p);
			uint l1 = editor.p_currentCat->getTileAt(1, p);
			const TileSet::TileInfo& i0 = editor.p_currentCat->getTileSet()->getTileInfo(l0);
			const TileSet::TileInfo& i1 = editor.p_currentCat->getTileSet()->getTileInfo(l1);
			if(i0.type == -1 && i1.type == -1) l0 = l1 = 0;
			Object* p_obj = editor.p_currentCat->getFrontObjectAt(p);

			if(l0 || l1 || p_obj)
			{
				// Die Auswahl ist gültig.
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

		if(editor.currentMode == 2 || editor.currentMode == 4)
		{
			// Rechteckpunkte von links oben nach rechts unten ordnen
			Vec2i pMin(min(editor.rectStart.x, editor.rectEnd.x), min(editor.rectStart.y, editor.rectEnd.y));
			Vec2i pMax(max(editor.rectStart.x, editor.rectEnd.x), max(editor.rectStart.y, editor.rectEnd.y));
			editor.rectStart = pMin;
			editor.rectEnd = pMax;
		}

		if(editor.currentMode == 2)
		{
			// Rechteck zeichnen
			if(editor.rectStart.x != -1)
			{
				editor.createUndoPoint();

				for(int x = editor.rectStart.x; x <= editor.rectEnd.x; x++)
				{
					for(int y = editor.rectStart.y; y <= editor.rectEnd.y; y++)
					{
						if(buttons & 1) editor.draw(Vec2i(x, y), shift);
						else if(buttons & 3) editor.clear(Vec2i(x, y));
					}
				}

				editor.rectStart = editor.rectEnd = Vec2i(-1, -1);
			}
		}

		if(editor.currentMode == 5)
		{
			// in den Zeichenmodus gehen
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
				if(editor.currentMode == 6)
				{
					editor.p_currentPin = 0;
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
									if(p_pin) editor.p_currentPin = p_pin;
								}
							}
						}
					}
				}
			}
		}

		oldCursor = p;
	}

	void onKeyEvent(const SDL_KeyboardEvent& event)
	{
		if(!getChild("SettingsPane")->isVisible() && !getChild("EditHintPane")->isVisible() && !getChild("MessageBoxPane")->isVisible())
		{
			// Uns interessiert nur, ob eine Taste gedrückt wurde.
			if(event.type != SDL_KEYDOWN) return;

			// Shift, Strg gedrückt?
			bool shift = (event.keysym.mod & KMOD_LSHIFT) || (event.keysym.mod & KMOD_RSHIFT);
			bool ctrl = (event.keysym.mod & KMOD_LCTRL) || (event.keysym.mod & KMOD_RCTRL);

			switch(event.keysym.sym)
			{
			case SDLK_ESCAPE:
				getChild("MenuPane.Menu")->focus();
				break;
			case SDLK_s:
				if(ctrl) handleClick(getChild("MenuPane.Menu.Save"));
				break;
			case SDLK_l:
				handleClick(getChild("Layer"));
				break;
			case SDLK_F5:
				handleClick(getChild("MenuPane.Menu.Play"));
				break;
			case SDLK_F10:
				handleClick(getChild("Refresh"));
				break;
			case SDLK_1: editor.setMode(0); break;
			case SDLK_2: editor.setMode(1); break;
			case SDLK_3: editor.setMode(2); break;
			case SDLK_4: editor.setMode(3); break;
			case SDLK_5: editor.setMode(4); break;
			case SDLK_6: editor.setMode(5); break;
			case SDLK_7: editor.setMode(6); break;
			case SDLK_y:
				if(ctrl) editor.undo();
				break;
			case SDLK_z:
				if(ctrl) editor.redo();
				break;
			case SDLK_c:
				if(ctrl) editor.copy();
				break;
			case SDLK_v:
				if(ctrl)
				{
					editor.createUndoPoint();
					if(!editor.paste(editor.rectStart)) editor.deleteLastUndoPoint();
				}
				break;
			case SDLK_x:
				if(ctrl)
				{
					if(!editor.copy()) break;
					editor.createUndoPoint();
					if(!editor.clear()) editor.deleteLastUndoPoint();
				}
				break;
			case SDLK_DELETE:
				editor.createUndoPoint();
				if(!editor.clear()) editor.deleteLastUndoPoint();
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

							editor.createUndoPoint();
							bool r = editor.copy();
							r &= editor.clear();
							r &= editor.paste(pMin + dir);

							delete[] editor.p_clipboard;
							editor.p_clipboard = p_oldClipboard;
							editor.clipboardSize = oldClipboardSize;

							if(!r) editor.undo();
						}
					}
					else
					{
						Vec2i p = editor.engine.getCursorPosition() / 16;
						if(p.x < editor.p_level->getSize().x && p.y < editor.p_level->getSize().y)
						{
							Object* p_obj = editor.p_level->getFrontObjectAt(p);
							if(p_obj)
							{
								Vec2i np = p + dir;
								if(np.x >= 0 && np.y >= 0 && np.x < editor.p_level->getSize().x && np.y < editor.p_level->getSize().y)
								{
									editor.createUndoPoint();
									if(shift)
									{
										// Objekt kopieren
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
			editor.currentLayer %= editor.p_level->getNumLayers();
		}
		else if(name == "LevelEditor.NumDiamondsNeeded-")
		{
			int n = editor.p_level->getNumDiamondsNeeded();
			if(n)
			{
				n--;
				editor.p_level->setNumDiamondsNeeded(n);
			}
		}
		else if(name == "LevelEditor.NumDiamondsNeeded+")
		{
			editor.p_level->setNumDiamondsNeeded(editor.p_level->getNumDiamondsNeeded() + 1);
		}
		else if(name == "LevelEditor.ElectricityOn")
		{
			bool on = static_cast<GUI_CheckBox*>(p_element)->isChecked();
			editor.createUndoPoint();
			editor.p_level->setElectricityOn(on);
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
			delete editor.p_level;
			editor.p_level = p_newLevel;

			// Meldung anzeigen
			editor.messageText = "$LE_INFO_GRAPHICS_RELOADED";
			editor.messageCounter = 100;
			editor.messageType = 0;
		}
		else if(name == "LevelEditor.Undo") editor.undo();
		else if(name == "LevelEditor.Redo") editor.redo();
		else if(name == "LevelEditor.ShowSettings")
		{
			static_cast<GUI_EditBox*>(getChild("SettingsPane.Settings.Title"))->setText(editor.p_level->getTitle());

			static_cast<GUI_CheckBox*>(getChild("SettingsPane.Settings.NightVision"))->check(editor.p_level->isNightVision());
			static_cast<GUI_CheckBox*>(getChild("SettingsPane.Settings.Rain"))->check(editor.p_level->isRaining());
			static_cast<GUI_CheckBox*>(getChild("SettingsPane.Settings.Clouds"))->check(editor.p_level->isCloudy());
			static_cast<GUI_CheckBox*>(getChild("SettingsPane.Settings.Snow"))->check(editor.p_level->isSnowing());
			static_cast<GUI_CheckBox*>(getChild("SettingsPane.Settings.Thunderstorm"))->check(editor.p_level->isThunderstorm());

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

			getChild("SettingsPane.Settings")->focus();
			editor.createUndoPoint();
		}
		else if(name == "LevelEditor.ShowMenu")
		{
			getChild("MenuPane.Menu")->focus();
		}
		else if(name == "LevelEditor.Cat0")
		{
			editor.currentCat = 0, editor.p_currentCat = editor.p_cat[0];
			updateToolTips();
		}
		else if(name == "LevelEditor.Cat1")
		{
			editor.currentCat = 1, editor.p_currentCat = editor.p_cat[1];
			updateToolTips();
		}
		else if(name == "LevelEditor.Cat2")
		{
			editor.currentCat = 2, editor.p_currentCat = editor.p_cat[2];
			updateToolTips();
		}
		else if(name == "LevelEditor.Cat3")
		{
			editor.currentCat = 3, editor.p_currentCat = editor.p_cat[3];
			updateToolTips();
		}
		else if(name == "LevelEditor.Cat4")
		{
			editor.currentCat = 4, editor.p_currentCat = editor.p_cat[4];
			updateToolTips();
		}
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
				editor.p_teleporter = 0;
				editor.p_hint = 0;
				editor.p_currentPin = editor.p_startPin = 0;
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
			editor.createUndoPoint();

			editor.p_level->clean();
			getChild("MenuPane")->hide();
			focus();

			editor.p_teleporter = 0;
			editor.p_hint = 0;
			editor.p_currentPin = editor.p_startPin = 0;
		}
		else if(name == "LevelEditor.MenuPane.Menu.Play")
		{
			getChild("MenuPane")->hide();
			focus();

			ParameterBlock p;
			p.set("levelDocument", editor.p_level->save());
			editor.engine.pushGameState("GS_Game", p);
			editor.engine.crossfade(new CF_Mosaic, 0.85);
		}
		else if(name == "LevelEditor.MenuPane.Menu.Help")
		{
			getChild("MenuPane.Menu")->hide();
			p_help->show(getChild("MenuPane.Menu"));
		}
		else if(name == "LevelEditor.MenuPane.Menu.Search")
		{
			getChild("SearchPane.Search")->focus();

			// Dateiliste füllen
			std::list<std::string> files = FileSystem::inst().listDirectory(FileSystem::inst().getAppHomeDirectory() + "levels");
			GUI_ListBox* p_listBox = static_cast<GUI_ListBox*>(getChild("SearchPane.Search.Files"));
			p_listBox->clear();
			for(std::list<std::string>::const_iterator i = files.begin(); i != files.end(); ++i)
			{
				std::string ext = getFilenameExtension(*i);
				if(ext == "xml")
				{
#ifdef CHECK_IF_IT_REALLY_IS_A_LEVEL
					// oberflächliche Prüfung, ob es eine Level-Datei ist
					TiXmlDocument doc;
					std::string str = FileSystem::inst().readStringFromFile(FileSystem::inst().getAppHomeDirectory() + "levels/" + *i);
					doc.Parse(str.c_str());

					if(doc.FirstChildElement("Level"))
#endif
					{
						GUI_ListBox::ListItem item(*i, 0);
						p_listBox->addItem(item);
					}
				}
			}
		}
		else if(name == "LevelEditor.MenuPane.Menu.Load")
		{
			std::string filename = static_cast<GUI_EditBox*>(getChild("MenuPane.Menu.Filename"))->getText();
			std::string path;
			if(!filename.empty())
			{
				path = FileSystem::inst().getAppHomeDirectory() + "levels/" + setFilenameExtension(filename, "xml");

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

							// Fehlermeldung anzeigen
							editor.messageText = "$LE_ERROR_LOADING";
							editor.messageCounter = 200;
							editor.messageType = 1;
						}
						else
						{
							delete editor.p_level;
							editor.p_level = p_newLevel;

							getChild("MenuPane")->hide();
							focus();

							editor.setSavePoint();

							editor.clearUndo();
							editor.clearRedo();

							editor.originalFilename = path;
							editor.p_teleporter = 0;
							editor.p_hint = 0;
							editor.p_currentPin = editor.p_startPin = 0;
							editor.setMode(0);
						}
					}
					else if(!confirmed)
					{
						getChild("MessageBoxPane.MessageBox.Text1")->show();
						getChild("MessageBoxPane.MessageBox.Text2")->hide();
						getChild("MessageBoxPane.MessageBox")->focus();
						p_clickWhenConfirmed = p_element;
					}
				}
				else
				{
					// Fehlermeldung anzeigen
					editor.messageText = "$LE_ERROR_FILE_DOESNT_EXIST";
					editor.messageCounter = 200;
					editor.messageType = 1;
				}
			}
		}
		else if(name == "LevelEditor.MenuPane.Menu.Save")
		{
			std::string filename = static_cast<GUI_EditBox*>(getChild("MenuPane.Menu.Filename"))->getText();
			std::string path;
			if(!filename.empty())
			{
				path = FileSystem::inst().getAppHomeDirectory() + "levels/" + setFilenameExtension(filename, "xml");

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

						// Meldung anzeigen
						editor.messageText = "$LE_INFO_LEVEL_SAVED";
						editor.messageCounter = 100;
						editor.messageType = 0;
					}
					else
					{
						// Fehlermeldung anzeigen
						editor.messageText = "$LE_ERROR_SAVING";
						editor.messageCounter = 200;
						editor.messageType = 1;
					}
				}
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
				editor.engine.crossfade(new CF_Star, 0.85);
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
			SDL_Event event;
			event.type = SDL_QUIT;
			SDL_PushEvent(&event);
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

			getChild("SettingsPane")->hide();
			focus();

			if(r) handleClick(getChild("Refresh"));
		}
		else if(name == "LevelEditor.SettingsPane.Settings.Cancel")
		{
			editor.undo();
			getChild("SettingsPane")->hide();
			focus();
		}

		if(name == "LevelEditor.EditHintPane.EditHint.OK")
		{
			editor.p_hint->setText(static_cast<GUI_MultiLineEditBox*>(getChild("EditHintPane.EditHint.Text"))->getText());
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
			editor.undo();
			getChild("EditHintPane")->hide();
			focus();
		}

		if(name == "LevelEditor.MessageBoxPane.MessageBox.Yes")
		{
			getChild("MessageBoxPane")->hide();
			confirmed = true;
			handleClick(p_clickWhenConfirmed);
			confirmed = false;
		}
		else if(name == "LevelEditor.MessageBoxPane.MessageBox.No")
		{
			getChild("MessageBoxPane")->hide();
			static_cast<GUI_EditBox*>(getChild("MenuPane.Menu.Filename"))->setText(editor.originalFilename);
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
	GS_LevelEditor& editor;
	Vec2i oldCursor;
	bool confirmed;
	GUI_Element* p_clickWhenConfirmed;
	bool realDown;
	GUI_Element* p_toolTip[24][3];
	Help* p_help;
};

GS_LevelEditor::GS_LevelEditor() : GameState("GS_LevelEditor"), engine(Engine::inst()), pipetteObjectAttributes("")
{
}

GS_LevelEditor::~GS_LevelEditor()
{
}

void GS_LevelEditor::onRender()
{
	// Level rendern
	p_level->render();

	if(currentMode == 6)
	{
		// ausgewählten Pin und Start-Pin hervorheben

		glDisable(GL_LINE_SMOOTH);
		glDisable(GL_TEXTURE_2D);
		glLineWidth(1.0f);
		glPushMatrix();
		glTranslated(0.5, 0.5, 0.0);

		if(p_currentPin)
		{
			glBegin(GL_LINE_LOOP);
			glColor4d(0.0, 0.0, 1.0, 0.75);
			Vec2i p = p_currentPin->getScreenPosition();
			glVertex2i(p.x - 2, p.y - 2);
			glVertex2i(p.x + 2, p.y - 2);
			glVertex2i(p.x + 2, p.y + 2);
			glVertex2i(p.x - 2, p.y + 2);
			glEnd();
		}

		if(p_startPin)
		{
			glBegin(GL_LINE_LOOP);
			glColor4d(0.5, 0.5, 1.0, 1.0);
			Vec2i p = p_startPin->getScreenPosition();
			glVertex2i(p.x - 2, p.y - 2);
			glVertex2i(p.x + 2, p.y - 2);
			glVertex2i(p.x + 2, p.y + 2);
			glVertex2i(p.x - 2, p.y + 2);
			glEnd();
		}

		glPopMatrix();
		glEnable(GL_LINE_SMOOTH);
	}

	// Hintergrundverlauf rendern
	glBegin(GL_QUADS);
	glColor4d(0.15, 0.2, 0.2, 1.0);
	glVertex2i(0, 400);
	glVertex2i(640, 400);
	glColor4d(0.4, 0.51, 0.25, 1.0);
	glVertex2i(640, 480);
	glVertex2i(0, 480);
	glEnd();

	if(rectStart.x != -1)
	{
		Vec2i pMin(min(rectStart.x, rectEnd.x), min(rectStart.y, rectEnd.y));
		Vec2i pMax(max(rectStart.x, rectEnd.x), max(rectStart.y, rectEnd.y));
		Vec2i p1 = pMin * 16;
		Vec2i p2 = pMax * 16 + Vec2i(16, 16);

		if(currentMode == 2)
		{
			// Rechteck rendern
			glBegin(GL_QUADS);
			if(drawStartButtons & 1) glColor4d(0.25, 0.25, 1.0, 0.4);
			else if(drawStartButtons & 3) glColor4d(1.0, 0.25, 0.25, 0.4);
			glVertex2i(p1.x, p1.y);
			glVertex2i(p2.x, p1.y);
			glVertex2i(p2.x, p2.y);
			glVertex2i(p1.x, p2.y);
			glEnd();
			glBegin(GL_LINE_LOOP);
			if(drawStartButtons & 1) glColor4d(0.25, 0.25, 1.0, 0.85);
			else if(drawStartButtons & 3) glColor4d(1.0, 0.25, 0.25, 0.85);
			glVertex2i(p1.x, p1.y);
			glVertex2i(p2.x, p1.y);
			glVertex2i(p2.x, p2.y);
			glVertex2i(p1.x, p2.y);
			glEnd();
		}
		else if(currentMode == 4)
		{
			// Rechteck rendern
			glEnable(GL_LINE_STIPPLE);
			glLineWidth(2.0f);
			glLineStipple(1, (0xF0F0 << ((engine.getTime() / 40) % 8)) % 0xFFFF);
			glBegin(GL_LINE_LOOP);
			glColor4d(0.0, 0.0, 0.0, 0.75);
			glVertex2i(p1.x + 1, p1.y + 1);
			glVertex2i(p2.x + 1, p1.y + 1);
			glVertex2i(p2.x + 1, p2.y + 1);
			glVertex2i(p1.x + 1, p2.y + 1);
			glEnd();
			glBegin(GL_LINE_LOOP);
			glColor4d(1.0, 1.0, 1.0, 0.75);
			glVertex2i(p1.x, p1.y);
			glVertex2i(p2.x, p1.y);
			glVertex2i(p2.x, p2.y);
			glVertex2i(p1.x, p2.y);
			glEnd();
			glLineWidth(1.0f);
			glDisable(GL_LINE_STIPPLE);
		}
	}

	// ausgewählte Kategorie rendern
	glPushMatrix();
	glTranslated(245.0, 428.0, 0.0);
	p_currentCat->render();

	// Auswahl rendern
	if(currentCat == currentBrush.z && !pipetteUsed)
	{
		glBegin(GL_LINE_LOOP);
		Vec2i p(currentBrush.x * 16, currentBrush.y * 16);
		glColor4d(1.0, 0.75, 0.75, 1.0);
		glVertex2i(p.x, p.y);
		glColor4d(1.0, 0.15, 0.15, 1.0);
		glVertex2i(p.x + 16, p.y);
		glVertex2i(p.x + 16, p.y + 16);
		glVertex2i(p.x, p.y + 16);
		glEnd();
	}

	glPopMatrix();

	if(p_hint)
	{
		p_hint->setText(static_cast<GUI_MultiLineEditBox*>(gui["LevelEditor.EditHintPane.EditHint.Text"])->getText());
		p_hint->render(43, Vec2i(0, 0), Vec4d(1.0));
	}

	if(messageCounter && !messageText.empty())
	{
		// Nachricht ausgeben
		glPushMatrix();
		int y = 0;
		if(messageCounter < 10) y = -40 + 4 * messageCounter;
		glTranslated(0.0, y, 0.0);

		Vec3d color(0.5, 0.5, 0.5);
		if(messageType == 0) color = Vec3d(0.0, 0.5, 0.0);
		else if(messageType == 1) color = Vec3d(0.5, 0.0, 0.0);

		glBegin(GL_QUADS);
		glColor4d(color.r, color.g, color.b, 0.75);
		glVertex2i(0, 0);
		glVertex2i(640, 0);
		glColor4d(color.r, color.g, color.b, 0.9);
		glVertex2i(640, 35);
		glVertex2i(0, 35);
		glEnd();
		glLineWidth(1.0f);
		glBegin(GL_LINES);
		glColor4d(0.0, 0.0, 0.0, 0.9);
		glVertex2i(0, 35);
		glVertex2i(640, 35);
		glEnd();

		gui.getFont()->renderText(localizeString(messageText), Vec2i(10, 9), Vec4d(1.0));

		glPopMatrix();
	}
}

void GS_LevelEditor::onUpdate()
{
	// alte Objekte löschen, neue Objekte hinzufügen
	p_level->removeOldObjects();
	p_level->addNewObjects();

	if(messageCounter) messageCounter--;
}

void GS_LevelEditor::onEnter(const ParameterBlock& context)
{
	// leeren Level laden
	p_level = new Level;
	p_level->setInEditor(true);
	p_level->load("level_default.xml");

	// Kategorien laden
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
	clipboardSize = Vec2i(0, 0);
	p_clipboard = 0;

	p_teleporter = 0;
	p_hint = 0;
	p_currentPin = 0;
	p_startPin = 0;

	originalFilename = "";
	setSavePoint();

	messageText = "";
	messageCounter = 0;
	messageType = 0;

	// Dialog erzeugen
	new LevelEditorGUI(*this);
}

void GS_LevelEditor::onLeave(const ParameterBlock& context)
{
	clearUndo();
	clearRedo();

	// Ressourcen löschen
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

	// Dialog löschen
	delete gui["LevelEditor"];
}

void GS_LevelEditor::onGetFocus()
{
	Engine::inst().playMusic("");

	gui["LevelEditor"]->focus();
}

void GS_LevelEditor::onLoseFocus()
{
	gui["LevelEditor"]->hide();
}

void GS_LevelEditor::createUndoPoint()
{
	clearRedo();
	undoList.push_front(p_level->save());

	// auf 64 Schritte begrenzen
	while(undoList.size() > 64)
	{
		delete undoList.back();
		undoList.erase(--undoList.end());
	}
}

void GS_LevelEditor::undo()
{
	if(!undoList.empty())
	{
		redoList.push_front(p_level->save());
		TiXmlDocument* p_old = undoList.front();
		undoList.erase(undoList.begin());

		Level* p_undone = new Level;
		p_undone->setInEditor(true);
		p_undone->load(p_old);

		delete p_old;
		delete p_level;
		p_level = p_undone;

		p_teleporter = 0;
		p_hint = 0;
	}
}

void GS_LevelEditor::redo()
{
	if(!redoList.empty())
	{
		undoList.push_front(p_level->save());
		TiXmlDocument* p_old = redoList.front();
		redoList.erase(redoList.begin());

		Level* p_redone = new Level;
		p_redone->setInEditor(true);
		p_redone->load(p_old);

		delete p_old;
		delete p_level;
		p_level = p_redone;

		p_teleporter = 0;
		p_hint = 0;
	}
}

void GS_LevelEditor::clearUndo()
{
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

void GS_LevelEditor::deleteLastUndoPoint()
{
	if(!undoList.empty())
	{
		delete undoList.front();
		undoList.erase(undoList.begin());
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
			// Es ist ein Tile ausgewählt. Welches?
			uint l0 = p_brushCat->getTileAt(0, brush);
			uint l1 = p_brushCat->getTileAt(1, brush);
			tile = l0 + l1;
		}

		// dieses Tile an der Stelle und dem gewählten Layer einsetzen
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
			// Es ist ein Objekt ausgewählt. Welches?
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
				// Schienen hier nicht löschen!
				p_level->clearPosition(where, "Rail");
				p_level->removeOldObjects();
			}
			else if(objectType == "Rail" && !shift)
			{
				// Lava hier nicht löschen
				p_level->clearPosition(where, "Lava");
				p_level->removeOldObjects();
			}
			else if(objectType == "Hint")
			{
				// Ist da schon ein Zettel?
				Object* p_oldObj = p_level->getFrontObjectAt(where);
				if(p_oldObj)
				{
					if(p_oldObj->getType() == "Hint")
					{
						if(currentMode == 0)
						{
							// Es war wohl unabsichtlich. Statt den Zettel zu löschen, gehen wir in den Modifizieren-Modus.
							oldMode = currentMode;
							setMode(1);
							modify(where, 1, false);
							return;
						}
						else
						{
							// nichts tun
							return;
						}
					}
				}

				// Kein Zettel da. Schienen hier nicht löschen!
				p_level->clearPosition(where, "Rail");
				p_level->removeOldObjects();
			}
			else if(!shift)
			{
				// Objekte an dieser Stelle löschen
				p_level->clearPosition(where);
				p_level->removeOldObjects();
			}

			// das Objekt an der gewählten Stelle einsetzen
			Object* p_newObj = p_level->getPresets()->instancePreset(objectType, where, &objectAttributes);

			// "eintüten"
			p_level->removeOldObjects();
			p_level->addNewObjects();

			if(currentMode == 0)
			{
				bool isTeleporter = objectType == "Teleporter";
				bool isHint = objectType == "Hint";
				if(isTeleporter || isHint)
				{
					createUndoPoint();

					// sofort in den Modifizieren-Modus gehen
					oldMode = currentMode;
					setMode(1);
					modify(where, 1, false);
				}
			}
		}
	}
}

void GS_LevelEditor::erase(const Vec2i& where,
						   bool shift)
{
	// Ist da ein Objekt?
	Object* p_obj = shift ? 0 : p_level->getFrontObjectAt(where);
	if(p_obj) p_level->removeObject(p_obj);
	else
	{
		// Tile auf aktuellem Layer löschen
		p_level->setTileAt(currentLayer, where, 0);
	}

	p_level->removeOldObjects();
}

void GS_LevelEditor::clear(const Vec2i& where,
						   bool allLayers)
{
	// Objekte löschen
	p_level->clearPosition(where);

	if(allLayers)
	{
		for(int layer = 0; layer < 2; layer++) p_level->setTileAt(layer, where, 0);
	}
	else
	{
		// Tile auf aktuellem Layer löschen
		p_level->setTileAt(currentLayer, where, 0);
	}

	p_level->removeOldObjects();
}

bool GS_LevelEditor::modify(const Vec2i& where,
							int buttons,
							bool shift)
{
	// Ist da ein Objekt?
	Object* p_obj = p_level->getFrontObjectAt(where);
	if(p_obj)
	{
		const std::string& type = p_obj->getType();
		if(type == "Teleporter")
		{
			p_teleporter = static_cast<Teleporter*>(p_obj);
			return true;
		}
		else if(type == "Hint")
		{
			p_hint = static_cast<Hint*>(p_obj);
			drawStartButtons = 0;
			gui["LevelEditor.EditHintPane.EditHint.Text"]->focus();
			static_cast<GUI_MultiLineEditBox*>(gui["LevelEditor.EditHintPane.EditHint.Text"])->setText(p_hint->getText());
			return true;
		}

		return p_obj->changeInEditor(shift ? 1 : 0);
	}
	else return false;
}

void GS_LevelEditor::transition(const Vec2i& where)
{
	uint t1, t2;
	uint l, r, t, b, tl, tr, bl, br, n_tl, n_tr, n_bl, n_br;

	for(int pass = 0; pass < 2; pass++)
	{
		if(pass == 0)
		{
			t1 = tileAtCat0(Vec2i(0, 0));
			t2 = tileAtCat0(Vec2i(1, 0));
			l = tileAtCat0(Vec2i(4, 1));
			r = tileAtCat0(Vec2i(2, 1));
			t = tileAtCat0(Vec2i(3, 2));
			b = tileAtCat0(Vec2i(3, 0));
			tl = tileAtCat0(Vec2i(4, 2));
			tr = tileAtCat0(Vec2i(2, 2));
			bl = tileAtCat0(Vec2i(4, 0));
			br = tileAtCat0(Vec2i(2, 0));
			n_tl = tileAtCat0(Vec2i(1, 2));
			n_tr = tileAtCat0(Vec2i(0, 2));
			n_bl = tileAtCat0(Vec2i(1, 1));
			n_br = tileAtCat0(Vec2i(0, 1));
		}
		else if(pass == 1)
		{
			t2 = tileAtCat0(Vec2i(0, 0));
			t1 = tileAtCat0(Vec2i(1, 0));
			r = tileAtCat0(Vec2i(4, 1));
			l = tileAtCat0(Vec2i(2, 1));
			b = tileAtCat0(Vec2i(3, 2));
			t = tileAtCat0(Vec2i(3, 0));
			n_tl = tileAtCat0(Vec2i(4, 2));
			n_tr = tileAtCat0(Vec2i(2, 2));
			n_bl = tileAtCat0(Vec2i(4, 0));
			n_br = tileAtCat0(Vec2i(2, 0));
			tl = tileAtCat0(Vec2i(1, 2));
			tr = tileAtCat0(Vec2i(0, 2));
			bl = tileAtCat0(Vec2i(1, 1));
			br = tileAtCat0(Vec2i(0, 1));
		}

		// Umgebung scannen
		int x = p_level->getTileAt(0, where + Vec2i(-1, 0));
		bool left = x == t1 || x == n_tl || x == n_bl || x == r;
		x = p_level->getTileAt(0, where + Vec2i(1, 0));
		bool right = x == t1 || x == n_tr || x == n_br || x == l;
		x = p_level->getTileAt(0, where + Vec2i(0, -1));
		bool top = x == t1 || x == n_tl || x == n_tr || x == b;
		x = p_level->getTileAt(0, where + Vec2i(0, 1));
		bool bottom = x == t1 || x == n_bl || x == n_br || x == t;
		x = p_level->getTileAt(0, where + Vec2i(-1, -1));
		bool topLeft = x == t1 || x == n_tl || x == n_tr || x == n_bl || x == br;
		x = p_level->getTileAt(0, where + Vec2i(1, -1));
		bool topRight = x == t1 || x == n_tl || x == n_tr || x == n_br || x == bl;
		x = p_level->getTileAt(0, where + Vec2i(-1, 1));
		bool bottomLeft = x == t1 || x == n_tl || x == n_bl || x == n_br || x == tr;
		x = p_level->getTileAt(0, where + Vec2i(1, 1));
		bool bottomRight = x == t1 || x == n_tr || x == n_bl || x == n_br || x == tl;

		uint result = ~0;

		if(!top && right && bottom && !left) result = n_tl;
		else if(!top && !right && bottom && left) result = n_tr;
		else if(top && right && !bottom && !left) result = n_bl;
		else if(top && !right && !bottom && left) result = n_br;
		else if(!top && !right && !bottom && left) result = l;
		else if(!top && right && !bottom && !left) result = r;
		else if(top && !right && !bottom && !left) result = t;
		else if(!top && !right && bottom && !left) result = b;
		else if(!top && !right && !bottom && !left)
		{
			if(topLeft && !topRight && !bottomLeft && !bottomRight) result = tl;
			else if(!topLeft && topRight && !bottomLeft && !bottomRight) result = tr;
			else if(!topLeft && !topRight && bottomLeft && !bottomRight) result = bl;
			else if(!topLeft && !topRight && !bottomLeft && bottomRight) result = br;
		}

		if(result != ~0)
		{
			p_level->setTileAt(0, where, result);
			return;
		}
	}
}

uint GS_LevelEditor::tileAtCat0(const Vec2i& where)
{
	return p_cat[0]->getTileAt(0, where);
}

bool GS_LevelEditor::copy()
{
	if(currentMode != 4 || rectStart.x < 0) return false;

	// alte Zwischenablage löschen
	delete[] p_clipboard;

	// Platz für die neue Zwischenablage schaffen
	Vec2i pMin(min(rectStart.x, rectEnd.x), min(rectStart.y, rectEnd.y));
	Vec2i pMax(max(rectStart.x, rectEnd.x), max(rectStart.y, rectEnd.y));
	clipboardSize = Vec2i(1, 1) + pMax - pMin;
	p_clipboard = new FieldInClipboard[clipboardSize.x * clipboardSize.y];

	// reinkopieren
	uint index = 0;
	for(int x = pMin.x; x <= pMax.x; x++)
	{
		for(int y = pMin.y; y <= pMax.y; y++)
		{
			Vec2i p(x, y);

			// Tiles kopieren
			for(int layer = 0; layer < 2; layer++) p_clipboard[index].tile[layer] = p_level->getTileAt(layer, p);

			// Objekte kopieren
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
	if(!p_level->isValidPosition(where) && !p_level->isValidPosition(where + clipboardSize + Vec2i(-1, -1))) return false;

	uint index = 0;
	for(int x = 0; x < clipboardSize.x; x++)
	{
		for(int y = 0; y < clipboardSize.y; y++)
		{
			Vec2i p = where + Vec2i(x, y);

			// Tiles einfügen
			for(int layer = 0; layer < 2; layer++)
			{
				uint tile = p_clipboard[index].tile[layer];
				if(tile) p_level->setTileAt(layer, p, tile);
			}

			if(!p_clipboard[index].objectTypes.empty())
			{
				// alte Objekte löschen
				p_level->clearPosition(p);
				p_level->removeOldObjects();

				// Objekte einfügen
				std::list<std::string>::const_iterator i;
				std::list<TiXmlElement>::iterator j;
				for(i = p_clipboard[index].objectTypes.begin(), j = p_clipboard[index].objectAttributes.begin();
					i != p_clipboard[index].objectTypes.end() && j != p_clipboard[index].objectAttributes.end();
					i++, j++)
				{
					Object* p_newObj = p_level->getPresets()->instancePreset(*i, p, &(*j));
					p_level->addNewObjects();
				}
			}

			index++;
		}
	}

	rectStart = where;
	rectEnd = where + clipboardSize + Vec2i(-1, -1);

	rectStart.x = clamp(rectStart.x, 0, p_level->getSize().x - 1);
	rectStart.y = clamp(rectStart.y, 0, p_level->getSize().y - 1);
	rectEnd.x = clamp(rectEnd.x, 0, p_level->getSize().x - 1);
	rectEnd.y = clamp(rectEnd.y, 0, p_level->getSize().y - 1);

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