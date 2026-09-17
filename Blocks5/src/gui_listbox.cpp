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
	Renderer& renderer = Renderer::inst();
	bool focused = isFocused() || p_scrollBar->isFocused();

	if(useSkin())
	{
		// draw the list box
		gui.renderFrame(Vec2i(0, 0), size - Vec2i(16, 0), focused ? Vec2i(48, 144) : Vec2i(0, 144));
	}
	else
	{
		// draw the background and the frame
		const Vec4f top = focused ? Vec4f(0.6f, 0.6f, 0.6f, 1.0f) : Vec4f(0.4f, 0.4f, 0.4f, 1.0f);
		const Vec4f bottom = focused ? Vec4f(0.5f, 0.5f, 0.5f, 1.0f) : Vec4f(0.3f, 0.3f, 0.3f, 1.0f);
		const Vec2f s = static_cast<Vec2f>(size);
		const Vec2f corners[4] = {Vec2f(0.0f, 0.0f), Vec2f(s.x, 0.0f), s, Vec2f(0.0f, s.y)};
		const Vec4f colors[4] = {top, top, bottom, bottom};
		renderer.quad(corners, colors);
		renderer.hairlineRect(Vec2f(0.0f, 0.0f), s, Vec4f(0.0f, 0.0f, 0.0f, 1.0f));
	}

	// The items and the selection clipped to the inside of the frame; the
	// scroll bar is a child and draws after this, outside the scope.
	Renderer::ScissorScope clip(getAbsPosition() + Vec2i(2, 2), size - Vec2i(4, 4));

	renderer.push();
	renderer.translate(0.0, -scroll);

	// render the list items
	int y = 2;
	int h = p_font->getLineHeight();
	for(std::vector<ListItem>::const_iterator i = items.begin(); i != items.end(); ++i)
	{
		p_font->renderText(localizeString(i->text), Vec2i(4, y), active ? Vec4d(1.0, 1.0, 1.0, 1.0) : Vec4d(0.5, 0.5, 0.5, 1.0));
		y += h;
	}

	// draw the selection
	if(selection != -1)
	{
		const Vec4f top = focused ? Vec4f(0.25f, 0.25f, 1.0f, 0.5f) : Vec4f(0.25f, 0.25f, 0.7f, 0.5f);
		const Vec4f bottom = focused ? Vec4f(0.2f, 0.2f, 0.8f, 0.5f) : Vec4f(0.15f, 0.15f, 0.5f, 0.5f);
		int y = selection * h + 2;
		const float y0 = static_cast<float>(y), y1 = static_cast<float>(y + h), right = static_cast<float>(size.x - 2);
		const Vec2f corners[4] = {Vec2f(2.0f, y0), Vec2f(right, y0), Vec2f(right, y1), Vec2f(2.0f, y1)};
		const Vec4f colors[4] = {top, top, bottom, bottom};
		renderer.quad(corners, colors);
	}

	renderer.pop();
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
					// double click
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
				// first click
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

	// Only a key press matters here.
	if(event.type != SDL_KEYDOWN) return;

	switch(event.keysym.sym)
	{
	case SDLK_TAB:
	case SDLK_ESCAPE:
		// forward the event to the parent element - Escape belongs to the
		// dialog, not to the list.
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
	case SDLK_KP_ENTER:
		// As in the edit box: with no button of its own for it, Return
		// belongs to the dialog. And the button only on a fresh press.
		if(!items.empty() && selection != -1 && p_submitButton) { if(!GUI::inst().isKeyRepeat()) p_submitButton->click(); }
		else if(p_parent) p_parent->onKeyEvent(event);
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
		// make sure the selection is visible
		int h = p_font->getLineHeight();
		int sy = 2 + selection * h;
		int vsy = sy - scroll;
		if(vsy < 0) scroll += vsy;
		else if(vsy > size.y - h) scroll += vsy - (size.y - h);
		if(scroll < 0) scroll = 0;
	}

	updateScrollBar();

	// fire the signal
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