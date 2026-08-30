#include "pch.h"
#include "gs_campaigneditor.h"
#include "level.h"
#include "campaign.h"
#include "texture.h"
#include "gui_all.h"
#include "cf_all.h"
#include "filesystem.h"
#ifdef __EMSCRIPTEN__
#include "web_transfer.h"
#endif

namespace
{
	// Die Listbox jagt jeden Eintrag durch localizeString: ein fuehrendes '$'
	// waere eine String-ID, 0xA7 eine Sprachmarke. Der Name eines Levels
	// stammt bei einer fremden Kampagne aus einer fremden Datei - fuer die
	// ANZEIGE also entschaerfen. Gespeichert wird er unveraendert.
	std::string displaySafe(const std::string& name)
	{
		std::string result(name, 0, min<size_t>(name.length(), 64));
		for(size_t i = 0; i < result.length(); i++)
		{
			const unsigned char c = static_cast<unsigned char>(result[i]);
			if(c < 0x20 || c == 0xA7) result[i] = '_';
		}
		if(!result.empty() && result[0] == '$') result[0] = '_';
		return result.empty() ? std::string("?") : result;
	}

	std::string levelListLabel(const Campaign::LevelRef& ref)
	{
		std::string label(displaySafe(ref.name));
		if(ref.fromArchive) label += " " + localizeString("$CE_LEVEL_IN_ARCHIVE");
		return label;
	}
}

class CampaignEditorGUI : public GUI_Element, public sigslot::has_slots<>
{
public:
	CampaignEditorGUI(GS_CampaignEditor& editor) : GUI_Element("CampaignEditor", 0, Vec2i(0, 0), Vec2i(640, 480)), editor(editor)
	{
		load("campaigneditor.xml");

		static_cast<GUI_Button*>(getChild("Search"))->connectClicked(this, &CampaignEditorGUI::handleClick);
		static_cast<GUI_Button*>(getChild("Load"))->connectClicked(this, &CampaignEditorGUI::handleClick);
		static_cast<GUI_Button*>(getChild("Save"))->connectClicked(this, &CampaignEditorGUI::handleClick);
		static_cast<GUI_Button*>(getChild("New"))->connectClicked(this, &CampaignEditorGUI::handleClick);
		static_cast<GUI_Button*>(getChild("Quit"))->connectClicked(this, &CampaignEditorGUI::handleClick);
		static_cast<GUI_Button*>(getChild("Add"))->connectClicked(this, &CampaignEditorGUI::handleClick);
		static_cast<GUI_Button*>(getChild("Remove"))->connectClicked(this, &CampaignEditorGUI::handleClick);
		static_cast<GUI_Button*>(getChild("Up"))->connectClicked(this, &CampaignEditorGUI::handleClick);
		static_cast<GUI_Button*>(getChild("Down"))->connectClicked(this, &CampaignEditorGUI::handleClick);
		static_cast<GUI_EditBox*>(getChild("Title"))->connectChanged(this, &CampaignEditorGUI::handleClick);
		static_cast<GUI_MultiLineEditBox*>(getChild("Description"))->connectChanged(this, &CampaignEditorGUI::handleClick);
		static_cast<GUI_Button*>(getChild("NumUnlockedLevels-"))->connectClicked(this, &CampaignEditorGUI::handleClick);
		static_cast<GUI_Button*>(getChild("NumUnlockedLevels+"))->connectClicked(this, &CampaignEditorGUI::handleClick);
		static_cast<GUI_CheckBox*>(getChild("BonusLevel"))->connectChanged(this, &CampaignEditorGUI::handleClick);

		// Export/Import gibt es nur im Browser.
		{
			GUI_Element* p_export = getChild("Export");
			GUI_Element* p_import = getChild("Import");
#ifdef __EMSCRIPTEN__
			if(p_export) static_cast<GUI_Button*>(p_export)->connectClicked(this, &CampaignEditorGUI::handleClick);
			if(p_import) static_cast<GUI_Button*>(p_import)->connectClicked(this, &CampaignEditorGUI::handleClick);
#else
			if(p_export) p_export->hide();
			if(p_import) p_import->hide();
#endif
		}

		static_cast<GUI_Button*>(getChild("MessageBoxPane.MessageBox.Yes"))->connectClicked(this, &CampaignEditorGUI::handleClick);
		static_cast<GUI_Button*>(getChild("MessageBoxPane.MessageBox.No"))->connectClicked(this, &CampaignEditorGUI::handleClick);

		static_cast<GUI_Button*>(getChild("SearchPane.Search.Select"))->connectClicked(this, &CampaignEditorGUI::handleClick);
		static_cast<GUI_Button*>(getChild("SearchPane.Search.Cancel"))->connectClicked(this, &CampaignEditorGUI::handleClick);

		updateGUI();

		confirmed = false;
		p_clickWhenConfirmed = 0;
		noUpdate = false;
	}

	~CampaignEditorGUI()
	{
	}

	void onKeyEvent(const SDL_KeyboardEvent& event)
	{
		if(!getChild("SearchPane")->isVisible() && !getChild("MessageBoxPane")->isVisible())
		{
			// Uns interessiert nur, ob eine Taste gedrueckt wurde.
			if(event.type != SDL_KEYDOWN) return;

			// Shift, Strg gedrueckt?
			bool shift = (event.keysym.mod & KMOD_LSHIFT) || (event.keysym.mod & KMOD_RSHIFT);
			bool ctrl = (event.keysym.mod & KMOD_LCTRL) || (event.keysym.mod & KMOD_RCTRL);

			switch(event.keysym.sym)
			{
			case SDLK_ESCAPE:
				handleClick(getChild("Quit"));
				break;
			default:
				GUI_Element::onKeyEvent(event);
				break;
			}
		}
	}

	void handleClick(GUI_Element* p_element)
	{
		const std::string& name = p_element->getFullName();

		if(name == "CampaignEditor.New")
		{
			if(!editor.wasChanged() || confirmed)
			{
				editor.p_campaign->clear();
				editor.setSavePoint();
				static_cast<GUI_EditBox*>(getChild("Filename"))->setText("");
				updateGUI();

				editor.originalFilename = "";
			}
			else
			{
				getChild("MessageBoxPane.MessageBox.Text1")->show();
				getChild("MessageBoxPane.MessageBox.Text2")->hide();
				getChild("MessageBoxPane.MessageBox")->focus();
				p_clickWhenConfirmed = p_element;
			}
		}
		else if(name == "CampaignEditor.Search")
		{
			getChild("SearchPane.Search")->focus();

			// Dateiliste fuellen
			std::list<std::string> files = FileSystem::inst().listDirectory(FileSystem::inst().getAppHomeDirectory() + "levels/campaigns");
			GUI_ListBox* p_listBox = static_cast<GUI_ListBox*>(getChild("SearchPane.Search.Files"));
			p_listBox->clear();
			for(std::list<std::string>::const_iterator i = files.begin(); i != files.end(); ++i)
			{
				std::string ext = getFilenameExtension(*i);
				if(ext == "zip")
				{
					if(FileSystem::inst().fileExists(FileSystem::inst().getAppHomeDirectory() + "levels/campaigns/" + *i + "/campaign.xml"))
					{
						GUI_ListBox::ListItem item(*i, 0);
						p_listBox->addItem(item);
					}
				}
			}
		}
		else if(name == "CampaignEditor.Load")
		{
			std::string filename = static_cast<GUI_EditBox*>(getChild("Filename"))->getText();
			std::string path;
			if(!filename.empty())
			{
				path = FileSystem::inst().getAppHomeDirectory() + "levels/campaigns/" + setFilenameExtension(filename, "zip");

				if(FileSystem::inst().fileExists(path))
				{
					if(!editor.wasChanged() || confirmed)
					{
						Campaign* p_newCampaign = new Campaign;
						if(!p_newCampaign->load(path))
						{
							delete p_newCampaign;
							path = "";
							static_cast<GUI_EditBox*>(getChild("Filename"))->setText("");

							// Fehlermeldung anzeigen
							editor.messageText = "$CE_ERROR_LOADING";
							editor.messageCounter = 200;
							editor.messageType = 1;
						}
						else
						{
							// pruefen, ob jeder Level wirklich lesbar ist -
							// als lose Datei oder als Mitglied des Archivs.
							std::string missing;
							if(!p_newCampaign->sourcesExist(missing))
							{
								printfLog("+ ERROR: Campaign \"%s\": level source \"%s\" is missing.\n",
										  path.c_str(), missing.c_str());
								delete p_newCampaign;
								path = "";
								static_cast<GUI_EditBox*>(getChild("Filename"))->setText("");

								// Fehlermeldung anzeigen
								editor.messageText = "$CE_ERROR_LEVELS_MISSING";
								editor.messageCounter = 200;
								editor.messageType = 1;
							}
							else
							{
								delete editor.p_campaign;
								editor.p_campaign = p_newCampaign;

								editor.setSavePoint();
								editor.originalFilename = path;
								updateGUI();
							}
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
			}
		}
		else if(name == "CampaignEditor.Save")
		{
			std::string filename = static_cast<GUI_EditBox*>(getChild("Filename"))->getText();
			std::string path;
			if(!filename.empty())
			{
				path = FileSystem::inst().getAppHomeDirectory() + "levels/campaigns/" + setFilenameExtension(filename, "zip");

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
					if(editor.p_campaign->save(path))
					{
						editor.setSavePoint();
						editor.originalFilename = path;
#ifdef __EMSCRIPTEN__
						// Sofort in die IndexedDB, nicht erst beim naechsten
						// Fuenf-Sekunden-Takt - so wie beim Level-Editor.
						WebTransfer::syncHome();
#endif

						// Meldung anzeigen
						editor.messageText = "$CE_INFO_CAMPAIGN_SAVED";
						editor.messageCounter = 100;
						editor.messageType = 0;
					}
					else
					{
						// Fehlermeldung anzeigen
						editor.messageText = "$CE_ERROR_SAVING";
						editor.messageCounter = 200;
						editor.messageType = 1;
					}
				}
			}
		}
#ifdef __EMSCRIPTEN__
		else if(name == "CampaignEditor.Export")
		{
			// Die ZIP-Datei, die "Save" erzeugt, IST das Austauschformat: sie
			// enthaelt campaign.xml, alle Levels als level_N.xml und die Musik.
			// Sie wird unveraendert weitergereicht, nicht neu gebaut.
			FileSystem& fs = FileSystem::inst();
			const std::string filename = static_cast<GUI_EditBox*>(getChild("Filename"))->getText();
			const std::string path = filename.empty() ? std::string("")
				: fs.getAppHomeDirectory() + "levels/campaigns/" + setFilenameExtension(filename, "zip");

			if(!path.empty() && fs.fileExists(path))
			{
				WebTransfer::download(path, sanitizeFilenameStem(filename, "campaign") + ".zip");
				editor.messageText = "$CE_INFO_CAMPAIGN_EXPORTED";
				editor.messageCounter = 100;
				editor.messageType = 0;
			}
			else
			{
				// Anders als beim Level gibt es hier nichts zu exportieren, solange
				// die Kampagne nicht gespeichert wurde: das ZIP ist die Quelle.
				editor.messageText = "$IMPORT_NOTHING_TO_EXPORT";
				editor.messageCounter = 200;
				editor.messageType = 1;
			}
		}
		else if(name == "CampaignEditor.Import")
		{
			// 32 MiB. Die mitgelieferte blocks.zip hat 8,3 MB (Musik dominiert).
			if(!WebTransfer::openPicker(WebTransfer::CHANNEL_CAMPAIGN, ".zip", 33554432u, "/blocks5_import.zip"))
			{
				editor.messageText = "$IMPORT_CLICK_AGAIN";
				editor.messageCounter = 150;
				editor.messageType = 1;
			}
		}
#endif
		else if(name == "CampaignEditor.Quit")
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
		else if(name == "CampaignEditor.Add")
		{
			GUI_ListBox* p_listBox = static_cast<GUI_ListBox*>(getChild("AvailableLevels"));
			int selected = p_listBox->getSelection();
			if(selected != -1)
			{
				GUI_ListBox::ListItem* p_item = p_listBox->getSelectedItem();
				const Campaign::LevelRef ref(Campaign::makeLooseRef(p_item->text));
				editor.p_campaign->addLevel(ref);
				static_cast<GUI_ListBox*>(getChild("CampaignLevels"))->addItem(GUI_ListBox::ListItem(levelListLabel(ref), 0));
				static_cast<GUI_ListBox*>(getChild("AvailableLevels"))->removeItem(selected);
			}
		}
		else if(name == "CampaignEditor.Remove")
		{
			GUI_ListBox* p_listBox = static_cast<GUI_ListBox*>(getChild("CampaignLevels"));
			int selected = p_listBox->getSelection();
			if(selected != -1)
			{
				// Ueber den Index, nicht ueber den Anzeigetext: zwei Levels
				// duerfen denselben Namen tragen. updateGUI baut danach beide
				// Listen neu auf - ein Level aus dem Archiv taucht dabei nicht
				// in der Auswahlliste auf, denn dort stehen nur lose Dateien.
				editor.p_campaign->removeLevelAt(selected);
				updateGUI();
			}
		}
		else if(name == "CampaignEditor.Up")
		{
			GUI_ListBox* p_listBox = static_cast<GUI_ListBox*>(getChild("CampaignLevels"));
			const std::vector<GUI_ListBox::ListItem>& items = p_listBox->getItems();
			int selected = p_listBox->getSelection();
			if(selected != -1 && selected != 0)
			{
				GUI_ListBox::ListItem thisItem = items[selected];
				p_listBox->addItem(thisItem, selected - 1);
				p_listBox->removeItem(selected + 1);
				p_listBox->setSelection(selected - 1);
				editor.p_campaign->swapLevels(selected, selected - 1);
			}
		}
		else if(name == "CampaignEditor.Down")
		{
			GUI_ListBox* p_listBox = static_cast<GUI_ListBox*>(getChild("CampaignLevels"));
			const std::vector<GUI_ListBox::ListItem>& items = p_listBox->getItems();
			int selected = p_listBox->getSelection();
			if(selected != -1 && selected != items.size() - 1)
			{
				GUI_ListBox::ListItem thisItem = items[selected];
				p_listBox->addItem(thisItem, selected + 2);
				p_listBox->removeItem(selected);
				p_listBox->setSelection(selected + 1);
				editor.p_campaign->swapLevels(selected, selected + 1);
			}
		}
		else if(name == "CampaignEditor.NumUnlockedLevels-")
		{
			int n = editor.p_campaign->getNumUnlockedLevels();
			if(n > 1)
			{
				editor.p_campaign->setNumUnlockedLevels(n - 1);
				updateGUI();
			}
		}
		else if(name == "CampaignEditor.NumUnlockedLevels+")
		{
			int n = editor.p_campaign->getNumUnlockedLevels();
			editor.p_campaign->setNumUnlockedLevels(n + 1);
			updateGUI();
		}
		else if(name == "CampaignEditor.BonusLevel")
		{
			if(!noUpdate) updateCampaign();
		}
		else if(name == "CampaignEditor.Title" ||
			    name == "CampaignEditor.Description")
		{
			// Aenderungen speichern
			if(!noUpdate) updateCampaign();
		}

		if(name == "CampaignEditor.MessageBoxPane.MessageBox.Yes")
		{
			getChild("MessageBoxPane")->hide();
			confirmed = true;
			handleClick(p_clickWhenConfirmed);
			confirmed = false;
		}
		else if(name == "CampaignEditor.MessageBoxPane.MessageBox.No")
		{
			getChild("MessageBoxPane")->hide();
		}

		if(name == "CampaignEditor.SearchPane.Search.Select")
		{
			GUI_ListBox* p_listBox = static_cast<GUI_ListBox*>(getChild("SearchPane.Search.Files"));
			GUI_ListBox::ListItem* p_item = p_listBox->getSelectedItem();
			if(p_item)
			{
				static_cast<GUI_EditBox*>(getChild("Filename"))->setText(p_item->text);
				getChild("SearchPane")->hide();
			}
		}
		else if(name == "CampaignEditor.SearchPane.Search.Cancel")
		{
			getChild("SearchPane")->hide();
		}
	}

	void updateGUI()
	{
		noUpdate = true;

		// Liste der verfuegbaren Levels aktualisieren
		listAvailableLevels();

		// Liste der Levels aktualisieren
		GUI_ListBox* p_listBox = static_cast<GUI_ListBox*>(getChild("CampaignLevels"));
		const std::vector<Campaign::LevelRef>& levels = editor.p_campaign->getLevels();
		p_listBox->clear();
		for(uint i = 0; i < levels.size(); i++) p_listBox->addItem(GUI_ListBox::ListItem(levelListLabel(levels[i]), 0));

		// Anzahl der freigeschalteten Levels aktualisieren
		char temp[256] = "";
		sprintf(temp, "%s: %d", localizeString("$CE_UNLOCKED_LEVELS").c_str(), editor.p_campaign->getNumUnlockedLevels());
		static_cast<GUI_StaticText*>(getChild("NumUnlockedLevels"))->setText(temp);

		// Bonuslevel aktualisieren
		static_cast<GUI_CheckBox*>(getChild("BonusLevel"))->setChecked(editor.p_campaign->hasBonusLevel());

		// Titel und Beschreibung aktualisieren
		static_cast<GUI_EditBox*>(getChild("Title"))->setText(editor.p_campaign->getTitle());
		static_cast<GUI_MultiLineEditBox*>(getChild("Description"))->setText(editor.p_campaign->getDescription());

		noUpdate = false;
	}

	void listAvailableLevels()
	{
		GUI_ListBox* p_listBox = static_cast<GUI_ListBox*>(getChild("AvailableLevels"));
		p_listBox->clear();

		// alle Levels auflisten
		std::list<std::string> files = FileSystem::inst().listDirectory(FileSystem::inst().getAppHomeDirectory() + "levels");
		for(std::list<std::string>::const_iterator i = files.begin(); i != files.end(); ++i)
		{
			if(!editor.p_campaign->hasLevel(*i))
			{
				std::string ext = getFilenameExtension(*i);
				if(ext == "xml")
				{
					GUI_ListBox::ListItem item(*i, 0);
					p_listBox->addItem(item);
				}
			}
		}
	}

	void updateCampaign()
	{
		// Die Level-Liste wird NICHT mehr aus der Listbox rekonstruiert: ein
		// Eintrag ist jetzt (Quelle, Mitglied) und nicht sein Anzeigetext.
		// Add, Remove, Up und Down aendern die Kampagne direkt.
		editor.p_campaign->setBonusLevel(static_cast<GUI_CheckBox*>(getChild("BonusLevel"))->isChecked());
		editor.p_campaign->setTitle(static_cast<GUI_EditBox*>(getChild("Title"))->getText());
		editor.p_campaign->setDescription(static_cast<GUI_MultiLineEditBox*>(getChild("Description"))->getText());
	}

#ifdef __EMSCRIPTEN__
	// Holt das Ergebnis des asynchronen Dateidialogs ab.
	void pollImport()
	{
		std::string untrustedName;
		const int status = WebTransfer::pollImport(WebTransfer::CHANNEL_CAMPAIGN, untrustedName);
		if(status == WebTransfer::IMPORT_IDLE) return;

		FileSystem& fs = FileSystem::inst();
		// Der Staging-Name MUSS auf .zip enden, sonst erkennt
		// FileSystem::convertPath das Archiv nicht.
		const std::string staging("/blocks5_import.zip");

		if(status != WebTransfer::IMPORT_OK)
		{
			fs.deleteFile(staging);
			if(status == WebTransfer::IMPORT_CANCELLED) return;
			editor.messageText = (status == WebTransfer::IMPORT_TOO_BIG) ? "$IMPORT_TOO_BIG"
							   : (status == WebTransfer::IMPORT_WRONG_TYPE) ? "$CE_ERROR_IMPORT_INVALID"
							   : "$IMPORT_FAILED";
			editor.messageCounter = 200;
			editor.messageType = 1;
			return;
		}

		// Pruefen, BEVOR irgendetwas ins Benutzerverzeichnis geht. Dieselbe
		// Pruefung benutzt die Levelauswahl.
		if(!Campaign::isImportableArchive(staging))
		{
			fs.deleteFile(staging);
			editor.messageText = "$CE_ERROR_IMPORT_INVALID";
			editor.messageCounter = 200;
			editor.messageType = 1;
			return;
		}

		const std::string name(Campaign::installArchive(staging, untrustedName));
		fs.deleteFile(staging);
		if(name.empty())
		{
			editor.messageText = "$IMPORT_FAILED";
			editor.messageCounter = 200;
			editor.messageType = 1;
			return;
		}

		// Sofort nach IndexedDB durchschreiben - sonst waere der Import bis zu
		// fuenf Sekunden lang nur im Arbeitsspeicher.
		WebTransfer::syncHome();

		listAvailableLevels();
		static_cast<GUI_EditBox*>(getChild("Filename"))->setText(name);
		editor.messageText = "$CE_INFO_CAMPAIGN_IMPORTED";
		editor.messageCounter = 150;
		editor.messageType = 0;
	}
#endif

private:
	GS_CampaignEditor& editor;
	bool confirmed;
	GUI_Element* p_clickWhenConfirmed;
	bool noUpdate;
};

GS_CampaignEditor::GS_CampaignEditor() : GameState("GS_CampaignEditor"), engine(Engine::inst())
{
}

GS_CampaignEditor::~GS_CampaignEditor()
{
}

void GS_CampaignEditor::onRender()
{
	// Hintergrundbild rendern
	p_background->bind();
	glBegin(GL_QUADS);
	glColor3d(1.0, 1.0, 1.0);
	glTexCoord2i(0, 0);
	glVertex2i(0, 0);
	glTexCoord2i(640, 0);
	glVertex2i(640, 0);
	glTexCoord2i(640, 480);
	glVertex2i(640, 480);
	glTexCoord2i(0, 480);
	glVertex2i(0, 480);
	glEnd();
	p_background->unbind();

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

void GS_CampaignEditor::onUpdate()
{
#ifdef __EMSCRIPTEN__
	if(GUI_Element* p_gui = gui["CampaignEditor"]) static_cast<CampaignEditorGUI*>(p_gui)->pollImport();
#endif

	if(messageCounter) messageCounter--;
}

void GS_CampaignEditor::onEnter(const ParameterBlock& context)
{
	// leere Kampagne erzeugen
	p_campaign = new Campaign;
	p_campaign->clear();

	// Bild laden
	p_background = Manager<Texture>::inst().request("campaigneditor.png");

	originalFilename = "";
	setSavePoint();

	messageText = "";
	messageCounter = 0;
	messageType = 0;

	// Dialog erzeugen
	new CampaignEditorGUI(*this);
}

void GS_CampaignEditor::onLeave(const ParameterBlock& context)
{
#ifdef __EMSCRIPTEN__
	// Einen noch offenen Dateidialog aufgeben, sonst blockiert er den Kanal
	// weiter - siehe GS_LevelEditor::onLeave.
	WebTransfer::abandon(WebTransfer::CHANNEL_CAMPAIGN, "/blocks5_import.zip");
#endif

	// Ressourcen loeschen
	delete p_campaign;
	p_background->release();
	p_campaign = 0;
	p_background = 0;

	// Dialog loeschen
	delete gui["CampaignEditor"];
}

void GS_CampaignEditor::onGetFocus()
{
	gui["CampaignEditor"]->focus();
}

void GS_CampaignEditor::onLoseFocus()
{
	gui["CampaignEditor"]->hide();
}

bool GS_CampaignEditor::wasChanged()
{
	return p_campaign->getStateString() != lastSavedXML;
}

void GS_CampaignEditor::setSavePoint()
{
	lastSavedXML = p_campaign->getStateString();
}