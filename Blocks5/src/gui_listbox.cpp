#include "pch.h"
#include "gui_listbox.h"
#include "gui_scrollbar.h"
#include "gui_button.h"
#include "engine.h"

IMPL_CTOR(GUI_ListBox)
{
	selection = -1;
	scroll = 0;

	p_scrollBar = new GUI_ScrollBar("ScrollBar", this, Vec2i(size.x - 16, 0), Vec2i(16, size.y));
	p_scrollBar->connectChanged(this, &GUI_ListBox::handleScrollBarChanged);
	updateScrollBar();

	p_submitButton = 0;
	doubleClickTime = 0;
	doubleClickItem = 0;
}

GUI_ListBox::~GUI_ListBox()
{
}

void GUI_ListBox::onRender()
{
	GUI& gui = GUI::inst();
	bool focused = isFocused() || p_scrollBar->isFocused();

	if(useSkin())
	{
		// Listenfeld zeichnen
		gui.renderFrame(Vec2i(0, 0), size - Vec2i(16, 0), focused ? Vec2i(48, 144) : Vec2i(0, 144));
	}
	else
	{
		// Hintergrund zeichnen
		glBegin(GL_QUADS);
		if(focused) glColor4d(0.6, 0.6, 0.6, 1.0);
		else glColor4d(0.4, 0.4, 0.4, 1.0);
		glVertex2i(0, 0);
		glVertex2i(size.x, 0);
		if(focused) glColor4d(0.5, 0.5, 0.5, 1.0);
		else glColor4d(0.3, 0.3, 0.3, 1.0);
		glVertex2i(size.x, size.y);
		glVertex2i(0, size.y);
		glEnd();

		// Rahmen zeichnen
		glColor4d(0.0, 0.0, 0.0, 1.0);
		glBegin(GL_LINE_LOOP);
		glVertex2i(0, 0);
		glVertex2i(size.x, 0);
		glVertex2i(size.x, size.y);
		glVertex2i(0, size.y);
		glEnd();
	}

	const Vec2i pos = getAbsPosition();
	glEnable(GL_SCISSOR_TEST);
	int h = gui.getRoot()->getSize().y;
	glScissor(pos.x + 2, h - pos.y - size.y + 2, size.x - 4, size.y - 4);

	glPushMatrix();
	glTranslated(0.0, -scroll, 0.0);

	// Listeneinträge rendern
	int y = 2;
	h = p_font->getLineHeight();
	for(std::vector<ListItem>::const_iterator i = items.begin(); i != items.end(); ++i)
	{
		p_font->renderText(localizeString(i->text), Vec2i(4, y), active ? Vec4d(1.0, 1.0, 1.0, 1.0) : Vec4d(0.5, 0.5, 0.5, 1.0));
		y += h;
	}

	// Auswahl zeichnen
	if(selection != -1)
	{
		glBegin(GL_QUADS);
		if(focused) glColor4d(0.25, 0.25, 1.0, 0.5);
		else glColor4d(0.25, 0.25, 0.7, 0.5);
		int y = selection * h + 2;
		glVertex2i(2, y);
		glVertex2i(size.x - 2, y);
		if(focused) glColor4d(0.2, 0.2, 0.8, 0.5);
		else glColor4d(0.15, 0.15, 0.5, 0.5);
		glVertex2i(size.x - 2, y + h);
		glVertex2i(2, y + h);
		glEnd();
	}

	glPopMatrix();
	glDisable(GL_SCISSOR_TEST);
}

void GUI_ListBox::onUpdate()
{
	if(doubleClickTime) doubleClickTime--;
}

void GUI_ListBox::onMouseDown(const Vec2i& position,
							  int buttons)
{
	if(active && (buttons & 1))
	{
		setSelection(getIndexAt(position));
		if(selection != -1)
		{
			if(doubleClickTime)
			{
				if(selection == doubleClickItem)
				{
					// Doppelklick
					if(p_submitButton) p_submitButton->click();
					doubleClickTime = 0;
				}
				else
				{
					doubleClickTime = 300 / Engine::inst().getLogicRate();
				}
			}
			else
			{
				// erster Klick
				doubleClickTime = 300 / Engine::inst().getLogicRate();
			}

			doubleClickItem = selection;
		}
	}
}

void GUI_ListBox::onMouseWheel(int dir)
{
	scroll += dir * 4 * p_font->getLineHeight();
	updateScrollBar();
}

void GUI_ListBox::onKeyEvent(const SDL_KeyboardEvent& event)
{
	if(!active) return;

	// Uns interessiert nur, ob eine Taste gedrückt wurde.
	if(event.type != SDL_KEYDOWN) return;

	// Shift gedrückt?
	bool shift = (event.keysym.mod & KMOD_LSHIFT) || (event.keysym.mod & KMOD_RSHIFT);

	switch(event.keysym.sym)
	{
	case SDLK_TAB:
		// Ereignis an das Elternelement weiterleiten
		if(p_parent) p_parent->onKeyEvent(event);
		break;
	case SDLK_UP:
		if(!items.empty() && selection != -1) setSelection(selection ? selection - 1 : selection);
		break;
	case SDLK_DOWN:
		if(!items.empty() && selection != -1) setSelection(selection < static_cast<int>(items.size() - 1) ? selection + 1 : selection);
		break;
	case SDLK_PAGEUP:
		if(!items.empty() && selection != -1) setSelection(max(0, selection - size.y / GUI::inst().getFont()->getLineHeight()));
		break;
	case SDLK_PAGEDOWN:
		if(!items.empty() && selection != -1) setSelection(min(static_cast<int>(items.size() - 1), selection + size.y / GUI::inst().getFont()->getLineHeight()));
		break;
	case SDLK_HOME:
		if(!items.empty()) setSelection(0);
		break;
	case SDLK_END:
		if(!items.empty()) setSelection(static_cast<int>(items.size() - 1));
		break;
	case SDLK_RETURN:
		if(!items.empty() && selection != -1 && p_submitButton) p_submitButton->click();
		break;
	}
}

void GUI_ListBox::addItem(const ListItem& item,
						  int where)
{
	if(where < 0) where = static_cast<int>(items.size());
	items.insert(items.begin() + where, item);

	if(selection != -1 && selection >= where) selection++;

	updateScrollBar();
}

void GUI_ListBox::removeItem(int where)
{
	if(where == -1) where = selection;
	if(where < 0 || where >= static_cast<int>(items.size())) return;

	items.erase(items.begin() + where);

	if(selection == where) selection = -1;
	else if(selection < where) selection--;

	updateScrollBar();
}

void GUI_ListBox::removeItem(const std::string& text)
{
	for(uint i = 0; i < items.size(); i++)
	{
		if(items[i].text == text)
		{
			removeItem(i);
			return;
		}
	}
}

int GUI_ListBox::findItem(const std::string& text)
{
	for(uint i = 0; i < items.size(); i++)
	{
		if(items[i].text == text) return i;
	}

	return -1;
}

void GUI_ListBox::clear()
{
	items.clear();
	selection = -1;
	updateScrollBar();
}

GUI_ListBox::ListItem* GUI_ListBox::getSelectedItem()
{
	if(selection == -1) return 0;
	else return &(items[selection]);
}

std::string GUI_ListBox::getSelectedItemText()
{
	ListItem* p_item = getSelectedItem();
	if(p_item) return p_item->text;
	else return "";
}

const std::vector<GUI_ListBox::ListItem>& GUI_ListBox::getItems() const
{
	return items;
}

void GUI_ListBox::setSelection(int selection)
{
	if(selection < 0 || selection >= static_cast<int>(items.size())) selection = -1;
	if(this->selection == selection)
	{
		updateScrollBar();
		return;
	}

	this->selection = selection;

	if(selection != -1)
	{
		// dafür sorgen, dass die Auswahl sichtbar ist
		int h = p_font->getLineHeight();
		int sy = 2 + selection * h;
		int vsy = sy - scroll;
		if(vsy < 0) scroll += vsy;
		else if(vsy > size.y - h) scroll += vsy - (size.y - h);
		if(scroll < 0) scroll = 0;
	}

	updateScrollBar();

	// Signal auslösen
	changed(this);
}

int GUI_ListBox::getIndexAt(const Vec2i& position)
{
	int index = (position.y - 2 + scroll) / p_font->getLineHeight();
	if(index < 0 || index >= static_cast<int>(items.size())) index = -1;
	return index;
}

void GUI_ListBox::updateScrollBar()
{
	int h = p_font->getLineHeight();
	p_scrollBar->setAreaSize(static_cast<int>(items.size()) * h);
	p_scrollBar->setPageSize(size.y - 4);
	p_scrollBar->setScroll(scroll);
}

void GUI_ListBox::handleScrollBarChanged(GUI_Element* p_element)
{
	scroll = p_scrollBar->getScroll();
}

void GUI_ListBox::readAttributes(TiXmlElement* p_element)
{
	TiXmlElement* e = p_element->FirstChildElement("Item");
	while(e)
	{
		addItem(e->GetText());
		e = e->NextSiblingElement("Item");
	}

	e = p_element->FirstChildElement("SubmitButton");
	if(e)
	{
		GUI_Button* p_button = static_cast<GUI_Button*>(p_parent->getChild(e->GetText()));
		setSubmitButton(p_button);
	}
}