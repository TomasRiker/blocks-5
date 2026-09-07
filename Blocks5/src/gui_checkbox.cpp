#include "pch.h"
#include "gui_checkbox.h"
#include "engine.h"

IMPL_CTOR(GUI_CheckBox)
{
	title = "CheckBox";
	checked = false;
	newChecked = false;
	pushed = false;
	mouseOver = false;
}

GUI_CheckBox::~GUI_CheckBox()
{
}

void GUI_CheckBox::onRender()
{
	GUI& gui = GUI::inst();

	if(useSkin())
	{
		// draw the background
		gui.renderFrame(Vec2i(0, 0), size, pushed && mouseOver ? Vec2i(144, 0) : Vec2i(96, 0));

		if(checked)
		{
			// draw the checkmark
			Vec2i offset = (size - Vec2i(16, 16)) / 2;
			Engine::inst().renderSprite(gui.getSkin(), offset, Vec2i(32, 224), Vec2i(16, 16), Vec4d(1.0));
		}
	}
	else
	{
		// draw the background
		glBegin(GL_QUADS);
		if(pushed && mouseOver) glColor4d(0.9, 0.9, 0.9, 1.0);
		else glColor4d(0.75, 0.75, 0.75, 1.0);
		glVertex2i(0, 0);
		glVertex2i(size.x, 0);
		if(pushed && mouseOver) glColor4d(0.8, 0.8, 0.8, 1.0);
		else glColor4d(0.65, 0.65, 0.65, 1.0);
		glVertex2i(size.x, size.y);
		glVertex2i(0, size.y);

		if(checked)
		{
			// draw the "checkmark"
			glColor4d(0.0, 0.0, 0.0, 1.0);
			glVertex2i(4, 4);
			glVertex2i(size.x - 3, 4);
			glColor4d(0.25, 0.25, 0.25, 1.0);
			glVertex2i(size.x - 3, size.y - 3);
			glVertex2i(4, size.y - 3);
		}

		glEnd();

		// draw the frame
		glColor4d(0.0, 0.0, 0.0, 1.0);
		glBegin(GL_LINE_LOOP);
		glVertex2i(0, 0);
		glVertex2i(size.x, 0);
		glVertex2i(size.x, size.y);
		glVertex2i(0, size.y);
		glEnd();
	}

	// write the title
	Vec2i dim;
	std::string title = localizeString(this->title);
	p_font->measureText(title, &dim, 0);
	p_font->renderText(title, Vec2i(size.x + 10, (size.y - dim.y) / 2), active ? Vec4d(1.0, 1.0, 1.0, 1.0) : Vec4d(0.5, 0.5, 0.5, 1.0));
}

void GUI_CheckBox::onMouseDown(const Vec2i& position,
							   int buttons)
{
	if(active && (buttons & 1))
	{
		pushed = true;
		newChecked = !checked;
	}
}

void GUI_CheckBox::onMouseUp(const Vec2i& position,
							 int buttons)
{
	if(pushed && (buttons & 1))
	{
		pushed = false;

		if(mouseOver)
		{
			// fire the signal
			checked = newChecked;
			changed(this);
		}
	}
}

// The caption belongs to the control. It is drawn in onRender() at
// Vec2i(size.x + 10, ...), and exactly that strip counts here - a click on
// the text toggles, as <label for="..."> does in a browser.
//
// Measured, not guessed: a strip wider than the text would steal clicks
// from whatever stands to the right of it (options.xml puts language and
// detail in three tight columns). An empty title measures 0, leaving just
// the box - the filter buttons with their own <For> caption are unaffected.
bool GUI_CheckBox::containsPoint(const Vec2i& position)
{
	if(GUI_Element::containsPoint(position)) return true;
	if(title.empty()) return false;

	Vec2i dim;
	p_font->measureText(localizeString(title), &dim, 0);
	if(dim.x <= 0) return false;

	const int left = size.x + 10;
	return position.x >= left && position.x < left + dim.x &&
		   position.y >= 0 && position.y < max(size.y, dim.y);
}

void GUI_CheckBox::onMouseEnter(int buttons)
{
	mouseOver = true;
}

void GUI_CheckBox::onMouseLeave(int buttons)
{
	mouseOver = false;
}

void GUI_CheckBox::check(bool check)
{
	if(checked == check) return;
	checked = check;

	// fire the signal
	changed(this);
}

void GUI_CheckBox::readAttributes(TiXmlElement* p_element)
{
	TiXmlElement* e = p_element->FirstChildElement("Title");
	if(e)
	{
		const char* p_title = e->GetText();
		setTitle(p_title ? p_title : "");
	}

	e = p_element->FirstChildElement("Checked");
	if(e) check(true);
}