#include "pch.h"
#include "gui_radiobutton.h"
#include "engine.h"
#include "texture.h"

IMPL_CTOR(GUI_RadioButton)
{
	title = "RadioButton";
	group = 0;
	buttonLook = false;
	checked = false;
	pushed = false;
	mouseOver = false;

	positionOnTexture = Vec2i(0, 0);
	p_image = 0;
}

GUI_RadioButton::~GUI_RadioButton()
{
	if(p_image) p_image->release();
}

void GUI_RadioButton::onRender()
{
	GUI& gui = GUI::inst();

	if(buttonLook)
	{
		if(useSkin())
		{
			// render the button
			gui.renderFrame(Vec2i(0, 0), size, checked || (pushed && mouseOver) ? Vec2i(48, 96) : Vec2i(0, 96));
		}
		else
		{
			// draw the background and the frame
			Renderer& renderer = Renderer::inst();
			Vec4f top(0.75f, 0.75f, 0.75f, 1.0f), bottom(0.65f, 0.65f, 0.65f, 1.0f);
			if(pushed && mouseOver) top = Vec4f(0.9f, 0.9f, 0.9f, 1.0f), bottom = Vec4f(0.8f, 0.8f, 0.8f, 1.0f);
			else if(checked) top = Vec4f(1.0f, 1.0f, 1.0f, 1.0f), bottom = Vec4f(0.85f, 0.85f, 0.85f, 1.0f);
			const Vec2f s = static_cast<Vec2f>(size);
			const Vec2f corners[4] = {Vec2f(0.0f, 0.0f), Vec2f(s.x, 0.0f), s, Vec2f(0.0f, s.y)};
			const Vec4f colors[4] = {top, top, bottom, bottom};
			renderer.quad(corners, colors);
			renderer.hairlineRect(Vec2f(0.0f, 0.0f), s, Vec4f(0.0f, 0.0f, 0.0f, 1.0f));
		}

		// write the title
		Vec2i dim;
		std::string title = localizeString(this->title);
		p_font->measureText(title, &dim, 0);
		p_font->renderText(title, (size - dim) / 2, active ? Vec4f(1.0f, 1.0f, 1.0f, 1.0f) : Vec4f(0.5f, 0.5f, 0.5f, 1.0f));

		if(p_image)
		{
			// render the image
			Engine::inst().renderSprite(p_image, Vec2i(0, 0), positionOnTexture, size, Vec4f(1.0f));
		}
	}
	else
	{
		if(useSkin())
		{
			// draw the background
			gui.renderFrame(Vec2i(0, 0), size, pushed && mouseOver ? Vec2i(144, 0) : Vec2i(96, 0));

			if(checked)
			{
				// draw the checkmark
				Vec2i offset = (size - Vec2i(16, 16)) / 2;
				Engine::inst().renderSprite(gui.getSkin(), offset, Vec2i(48, 224), Vec2i(16, 16), Vec4f(1.0f));
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
		p_font->renderText(title, Vec2i(size.x + 10, (size.y - dim.y) / 2), active ? Vec4f(1.0f, 1.0f, 1.0f, 1.0f) : Vec4f(0.5f, 0.5f, 0.5f, 1.0f));
	}
}

void GUI_RadioButton::onMouseDown(const Vec2i& position,
								  int buttons)
{
	if(active && (buttons & 1)) pushed = true;
}

void GUI_RadioButton::onMouseUp(const Vec2i& position,
								int buttons)
{
	if(pushed && (buttons & 1))
	{
		pushed = false;

		if(mouseOver)
		{
			// check() is what fires the signal - it means "the user clicked",
			// as against setChecked(), which means "the display caught up".
			// Firing a second one here made a click that selects report twice
			// and a click on the one already selected report a change that did
			// not happen; a handler that counts, toggles or takes an undo point
			// would have believed both.
			check();
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
bool GUI_RadioButton::containsPoint(const Vec2i& position)
{
	if(GUI_Element::containsPoint(position)) return true;

	// In the button look the caption sits in the middle, not beside it.
	if(buttonLook) return false;

	if(title.empty()) return false;

	Vec2i dim;
	p_font->measureText(localizeString(title), &dim, 0);
	if(dim.x <= 0) return false;

	const int left = size.x + 10;
	return position.x >= left && position.x < left + dim.x &&
		   position.y >= 0 && position.y < max(size.y, dim.y);
}

void GUI_RadioButton::onMouseEnter(int buttons)
{
	mouseOver = true;
}

void GUI_RadioButton::onMouseLeave(int buttons)
{
	mouseOver = false;
}

void GUI_RadioButton::setChecked()
{
	if(checked) return;

	checked = true;

	// switch off every other radio button belonging to the same group
	const std::list<GUI_Element*>& siblings = p_parent->getChildren();
	for(std::list<GUI_Element*>::const_iterator i = siblings.begin(); i != siblings.end(); ++i)
	{
		GUI_Element* p_element = *i;
		if(p_element->getType() == getType() && p_element != this)
		{
			GUI_RadioButton* p_rb = static_cast<GUI_RadioButton*>(p_element);
			if(p_rb->getGroup() == getGroup()) p_rb->checked = false;
		}
	}
}

void GUI_RadioButton::check()
{
	if(checked) return;
	setChecked();

	// fire the signal
	changed(this);
}

void GUI_RadioButton::setGroup(uint group)
{
	if(this->group == group) return;

	this->group = group;
	checked = false;
}

void GUI_RadioButton::readAttributes(TiXmlElement* p_element)
{
	TiXmlElement* e = p_element->FirstChildElement("Title");
	if(e)
	{
		const char* p_title = e->GetText();
		setTitle(p_title ? p_title : "");
	}

	e = p_element->FirstChildElement("Group");
	if(e)
	{
		const char* p_text = e->GetText();
		if(p_text)
		{
			int group = 0;
			sscanf(p_text, "%d", &group);
			setGroup(group);
		}
	}

	e = p_element->FirstChildElement("Checked");
	if(e) check();

	e = p_element->FirstChildElement("ButtonLook");
	if(e) setButtonLook(true);

	e = p_element->FirstChildElement("Image");
	if(e)
	{
		const char* p_imageFilename = e->GetText();
		if(p_imageFilename) setRawImageFilename(p_imageFilename);

		e->QueryIntAttribute("u", &positionOnTexture.x);
		e->QueryIntAttribute("v", &positionOnTexture.y);
	}
}

void GUI_RadioButton::setImageFilename(const std::string& imageFilename)
{
	if(p_image) p_image->release();
	this->imageFilename = imageFilename;
	p_image = Manager<Texture>::inst().request(imageFilename);
}

void GUI_RadioButton::setRawImageFilename(const std::string& rawImageFilename)
{
	this->rawImageFilename = rawImageFilename;
	setImageFilename(localizeString(rawImageFilename));
}

// A $ID image follows a language switch, as GUI_Button's and
// GUI_StaticImage's do. The texture is requested only when a different name
// really comes out.
void GUI_RadioButton::onUpdate()
{
	if(rawImageFilename.empty()) return;

	const std::string wanted = localizeString(rawImageFilename);
	if(wanted != imageFilename) setImageFilename(wanted);
}