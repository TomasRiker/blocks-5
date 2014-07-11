#ifndef _GUI_LISTBOX_H
#define _GUI_LISTBOX_H

/*** Klasse für ein Listenfeld ***/

#include "gui_element.h"

class GUI_ScrollBar;
class GUI_Button;

class GUI_ListBox : public GUI_Element, public sigslot::has_slots<>
{
public:
	struct ListItem
	{
	public:
		ListItem(const char* p_text) : text(p_text), p_data(0) {}
		ListItem(const std::string& text, void* p_data) : text(text), p_data(p_data) {}
		std::string text;
		void* p_data;
	};

	DECL_CTOR(GUI_ListBox);
	~GUI_ListBox();

	void onRender();
	void onUpdate();
	void onMouseDown(const Vec2i& position, int buttons);
	void onMouseWheel(int dir);
	void onKeyEvent(const SDL_KeyboardEvent& event);
	INLINE_GETTYPE("GUI_ListBox");

	void addItem(const ListItem& item, int where = -1);
	void removeItem(int where);
	void removeItem(const std::string& text);
	int findItem(const std::string& text);
	void clear();
	INLINE_GETTER(int, getSelection, selection);
	ListItem* getSelectedItem();
	std::string getSelectedItemText();
	const std::vector<ListItem>& getItems() const;
	void setSelection(int selection);

	void readAttributes(TiXmlElement* p_element);

	INLINE_PGETTER(GUI_Button*, getSubmitButton, p_submitButton);
	INLINE_PSETTER(GUI_Button*, setSubmitButton, p_submitButton);
	INLINE_CONNECTOR(connectChanged, changed);

private:
	int getIndexAt(const Vec2i& position);
	void updateScrollBar();

	void handleScrollBarChanged(GUI_Element* p_element);

	std::vector<ListItem> items;
	int selection;
	int scroll;
	GUI_ScrollBar* p_scrollBar;
	int doubleClickTime;
	int doubleClickItem;
	GUI_Button* p_submitButton;

	sigslot::signal1<GUI_Element*> changed;
};

#endif