#include "pch.h"
#include "help.h"
#include "engine.h"
#include "filesystem.h"
#include "gui_all.h"

Help::Help(GUI_Element* p_parent) : GUI_Element("HelpPane", p_parent, Vec2i(0, 0), Vec2i(640, 480))
{
	load("help.xml");
	hide();

	static_cast<GUI_Button*>(getChild("Help.PreviousPage"))->connectClicked(this, &Help::handleClick);
	static_cast<GUI_Button*>(getChild("Help.NextPage"))->connectClicked(this, &Help::handleClick);
	static_cast<GUI_Button*>(getChild("Help.OK"))->connectClicked(this, &Help::handleClick);

	page = 1;
	p_focusWhenClosed = 0;
}

Help::~Help()
{
}

void Help::show(GUI_Element* p_focusWhenClosed)
{
	this->p_focusWhenClosed = p_focusWhenClosed;

	page = 1;
	updatePage();

	getChild("Help")->focus();
}

void Help::onKeyEvent(const SDL_KeyboardEvent& event)
{
	if(event.type == SDL_KEYDOWN && isVisible() && !GUI::inst().isKeyRepeat() &&
	   event.keysym.sym == SDLK_ESCAPE)
	{
		Engine::inst().consumeKeyPress(event.keysym.sym);
		handleClick(getChild("Help.OK"));
		return;
	}

	GUI_Element::onKeyEvent(event);
}

void Help::handleClick(GUI_Element* p_element)
{
	std::string name = p_element->getName();

	if(isVisible())
	{
		if(name == "PreviousPage")
		{
			if(page > 1)
			{
				page--;
				updatePage();
			}
		}
		else if(name == "NextPage")
		{
			if(page < 6)
			{
				page++;
				updatePage();
			}
		}
		else if(name == "OK")
		{
			hide();
			if(p_focusWhenClosed) p_focusWhenClosed->focus();
		}
	}
}

void Help::updatePage()
{
	char id[256] = "";
	sprintf(id, "$H_HELP_PAGE%d", page);
	static_cast<GUI_StaticText*>(getChild("Help.Page.Text"))->setText(id);
}