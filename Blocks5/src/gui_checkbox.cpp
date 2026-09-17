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
		Renderer& renderer = Renderer::inst();
		const bool lit = pushed && mouseOver;
		const Vec4f top = lit ? Vec4f(0.9f, 0.9f, 0.9f, 1.0f) : Vec4f(0.75f, 0.75f, 0.75f, 1.0f);
		const Vec4f bottom = lit ? Vec4f(0.8f, 0.8f, 0.8f, 1.0f) : Vec4f(0.65f, 0.65f, 0.65f, 1.0f);
		const Vec2f s = static_cast<Vec2f>(size);
		const Vec2f corners[4] = {Vec2f(0.0f, 0.0f), Vec2f(s.x, 0.0f), s, Vec2f(0.0f, s.y)};
		const Vec4f colors[4] = {top, top, bottom, bottom};
		renderer.quad(corners, colors);

		if(checked)
		{
			// draw the "checkmark"
			const Vec2f mark[4] = {Vec2f(4.0f, 4.0f), Vec2f(s.x - 3.0f, 4.0f), s - Vec2f(3.0f, 3.0f), Vec2f(4.0f, s.y - 3.0f)};
			const Vec4f markTop(0.0f, 0.0f, 0.0f, 1.0f), markBottom(0.25f, 0.25f, 0.25f, 1.0f);
			const Vec4f markColors[4] = {markTop, markTop, markBottom, markBottom};
			renderer.quad(mark, markColors);
		}

		// draw the frame
		renderer.hairlineRect(Vec2f(0.0f, 0.0f), s, Vec4f(0.0f, 0.0f, 0.0f, 1.0f));
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
// Measured, not assumed: a strip wider than the text would steal clicks from
// whatever stands to the right of it (options.xml puts language and detail
// radios in three tight columns). An empty title measures 0, leaving just the
// box - the filter buttons with a label of their own (for="...") are unaffected.
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

	// The display catching up with the file, not a click: nothing may fire.
	e = p_element->FirstChildElement("Checked");
	if(e) setChecked(true);
}