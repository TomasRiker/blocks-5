#include "pch.h"
#include "gs_campaigneditor.h"
#include "level.h"
#include "campaign.h"
#include "transfer.h"
#include "texture.h"
#include "gui_all.h"
#include "cf_all.h"
#include "filesystem.h"
#ifdef __EMSCRIPTEN__
#include "web_transfer.h"
#endif

namespace
{
	// The list box runs every entry through localizeString: a leading '$'
	// would be a string ID, 0xA7 a language tag. In somebody else's campaign
	// a level's name comes from somebody else's file - defuse it for DISPLAY.
	// It is saved unchanged.
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
			// Only a newly pressed key counts. A repeat is not a second
			// command.
			if(event.type != SDL_KEYDOWN || GUI::inst().isKeyRepeat()) return;

			switch(event.keysym.sym)
			{
			case SDLK_ESCAPE:
				// The key is spent here. Quit switches to GS_Menu, and
				// Engine::update() applies that in this same tick: the menu
				// would come up immediately afterwards, see the same Escape
				// through wasKeyPressed() and quit the game. For the same
				// reason as in Options::onKeyEvent().
				Engine::inst().consumeKeyPress(event.keysym.sym);
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

			// Fill the file list from both roots: the shipped campaign sits
			// with the game, the player's own with the player.
			const std::vector<std::string> files(Transfer::list(Transfer::KIND_CAMPAIGN));
			GUI_ListBox* p_listBox = static_cast<GUI_ListBox*>(getChild("SearchPane.Search.Files"));
			p_listBox->clear();
			for(std::vector<std::string>::const_iterator i = files.begin(); i != files.end(); ++i)
			{
				if(FileSystem::inst().fileExists(
					   FileSystem::inst().resolveContentPath("levels/campaigns/" + *i) + "/campaign.xml"))
				{
					GUI_ListBox::ListItem item(*i, 0);
					p_listBox->addItem(item);
				}
			}
		}
		else if(name == "CampaignEditor.Load")
		{
			std::string filename = static_cast<GUI_EditBox*>(getChild("Filename"))->getText();
			std::string path;
			if(!filename.empty())
			{
				path = FileSystem::inst().resolveContentPath("levels/campaigns/" + setFilenameExtension(filename, "zip"));

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
						}
						else
						{
							// check that every level really is readable - as a
							// loose file or as a member of the archive.
							std::string missing;
							if(!p_newCampaign->sourcesExist(missing))
							{
								printfLog("+ ERROR: Campaign \"%s\": level source \"%s\" is missing.\n",
										  path.c_str(), missing.c_str());
								delete p_newCampaign;
								path = "";
								static_cast<GUI_EditBox*>(getChild("Filename"))->setText("");

								Engine::inst().showToast(Engine::TOAST_ERROR, "$CE_ERROR_LEVELS_MISSING");
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
			else
			{
				// With no filename nothing would otherwise happen here at all -
				// the click would go nowhere and nobody would learn why.
				Engine::inst().showToast(Engine::TOAST_ERROR, "$ERROR_NO_FILENAME");
			}
		}
		else if(name == "CampaignEditor.Save")
		{
			std::string filename = static_cast<GUI_EditBox*>(getChild("Filename"))->getText();
			std::string path;
			if(!filename.empty())
			{
				const std::string basename(setFilenameExtension(filename, "zip"));

				// As in the level editor: saving goes to the user directory,
				// and never under a shipped name - such a campaign could never
				// be loaded again, because the game folder answers first.
				if(Transfer::isBuiltIn(Transfer::KIND_CAMPAIGN, basename))
				{
					Engine::inst().showToast(Engine::TOAST_ERROR, "$TR_ERROR_RESERVED");
					return;
				}

				path = FileSystem::inst().getAppHomeDirectory() + "levels/campaigns/" + basename;

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
						// Straight into the IndexedDB, not waiting for the next
						// five-second interval - as in the level editor.
						WebTransfer::syncHome();
#endif

						Engine::inst().showToast(Engine::TOAST_OK, "$CE_INFO_CAMPAIGN_SAVED");
					}
					else
					{
						Engine::inst().showToast(Engine::TOAST_ERROR, "$CE_ERROR_SAVING");
					}
				}
			}
			else
			{
				// With no filename nothing would otherwise happen here at all -
				// the click would go nowhere and nobody would learn why.
				Engine::inst().showToast(Engine::TOAST_ERROR, "$ERROR_NO_FILENAME");
			}
		}
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
				// By index, not by the displayed text: two levels may carry
				// the same name. updateGUI then rebuilds both lists - a level
				// from the archive does not appear in the selection list,
				// which holds only loose files.
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
			// save the changes
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

		// update the list of available levels
		listAvailableLevels();

		// update the list of levels
		GUI_ListBox* p_listBox = static_cast<GUI_ListBox*>(getChild("CampaignLevels"));
		const std::vector<Campaign::LevelRef>& levels = editor.p_campaign->getLevels();
		p_listBox->clear();
		for(uint i = 0; i < levels.size(); i++) p_listBox->addItem(GUI_ListBox::ListItem(levelListLabel(levels[i]), 0));

		// update the number of unlocked levels
		char temp[256] = "";
		sprintf(temp, "%s: %d", localizeString("$CE_UNLOCKED_LEVELS").c_str(), editor.p_campaign->getNumUnlockedLevels());
		static_cast<GUI_StaticText*>(getChild("NumUnlockedLevels"))->setText(temp);

		// update the bonus level
		static_cast<GUI_CheckBox*>(getChild("BonusLevel"))->setChecked(editor.p_campaign->hasBonusLevel());

		// update the title and description
		static_cast<GUI_EditBox*>(getChild("Title"))->setText(editor.p_campaign->getTitle());
		static_cast<GUI_MultiLineEditBox*>(getChild("Description"))->setText(editor.p_campaign->getDescription());

		noUpdate = false;
	}

	void listAvailableLevels()
	{
		GUI_ListBox* p_listBox = static_cast<GUI_ListBox*>(getChild("AvailableLevels"));
		p_listBox->clear();

		// list every level, from both roots
		const std::vector<std::string> files(Transfer::list(Transfer::KIND_LEVEL));
		for(std::vector<std::string>::const_iterator i = files.begin(); i != files.end(); ++i)
		{
			if(!editor.p_campaign->hasLevel(*i))
			{
				GUI_ListBox::ListItem item(*i, 0);
				p_listBox->addItem(item);
			}
		}
	}

	void updateCampaign()
	{
		// The level list is NOT rebuilt from the list box: an entry is
		// (source, member) and not its displayed text. Add, Remove, Up and
		// Down change the campaign directly.
		editor.p_campaign->setBonusLevel(static_cast<GUI_CheckBox*>(getChild("BonusLevel"))->isChecked());
		editor.p_campaign->setTitle(static_cast<GUI_EditBox*>(getChild("Title"))->getText());
		editor.p_campaign->setDescription(static_cast<GUI_MultiLineEditBox*>(getChild("Description"))->getText());
	}

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
	// render the background image
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
}

void GS_CampaignEditor::onUpdate()
{
}

void GS_CampaignEditor::onEnter(const ParameterBlock& context)
{
	// create an empty campaign
	p_campaign = new Campaign;
	p_campaign->clear();

	// load the image
	p_background = Manager<Texture>::inst().request("campaigneditor.png");

	originalFilename = "";
	setSavePoint();

	// create the dialog
	new CampaignEditorGUI(*this);
}

void GS_CampaignEditor::onLeave(const ParameterBlock& context)
{
	// free the resources
	delete p_campaign;
	p_background->release();
	p_campaign = 0;
	p_background = 0;

	// delete the dialog
	delete gui["CampaignEditor"];
}

void GS_CampaignEditor::onGetFocus()
{
	Engine::inst().playMusic("menu.ogg");

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