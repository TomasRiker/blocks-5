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
			// Button rendern
			gui.renderFrame(Vec2i(0, 0), size, checked || (pushed && mouseOver) ? Vec2i(48, 96) : Vec2i(0, 96));
		}
		else
		{
			// Hintergrund zeichnen
			glBegin(GL_QUADS);
			if(pushed && mouseOver) glColor4d(0.9, 0.9, 0.9, 1.0);
			else if(checked) glColor4d(1.0, 1.0, 1.0, 1.0);
			else glColor4d(0.75, 0.75, 0.75, 1.0);
			glVertex2i(0, 0);
			glVertex2i(size.x, 0);
			if(pushed && mouseOver) glColor4d(0.8, 0.8, 0.8, 1.0);
			else if(checked) glColor4d(0.85, 0.85, 0.85, 1.0);
			else glColor4d(0.65, 0.65, 0.65, 1.0);
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

		// Titel schreiben
		Vec2i dim;
		std::string title = localizeString(this->title);
		p_font->measureText(title, &dim, 0);
		p_font->renderText(title, (size - dim) / 2, active ? Vec4d(1.0, 1.0, 1.0, 1.0) : Vec4d(0.5, 0.5, 0.5, 1.0));

		if(p_image)
		{
			// Bild rendern
			Engine::inst().renderSprite(p_image, Vec2i(0, 0), positionOnTexture, size, Vec4d(1.0));
		}
	}
	else
	{
		if(useSkin())
		{
			// Hintergrund zeichnen
			gui.renderFrame(Vec2i(0, 0), size, pushed && mouseOver ? Vec2i(144, 0) : Vec2i(96, 0));

			if(checked)
			{
				// Häkchen zeichnen
				Vec2i offset = (size - Vec2i(16, 16)) / 2;
				Engine::inst().renderSprite(gui.getSkin(), offset, Vec2i(48, 224), Vec2i(16, 16), Vec4d(1.0));
			}
		}
		else
		{
			// Hintergrund zeichnen
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
				// "Häkchen" zeichnen
				glColor4d(0.0, 0.0, 0.0, 1.0);
				glVertex2i(4, 4);
				glVertex2i(size.x - 3, 4);
				glColor4d(0.25, 0.25, 0.25, 1.0);
				glVertex2i(size.x - 3, size.y - 3);
				glVertex2i(4, size.y - 3);
			}

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

		// Titel schreiben
		Vec2i dim;
		std::string title = localizeString(this->title);
		p_font->measureText(title, &dim, 0);
		p_font->renderText(title, Vec2i(size.x + 10, (size.y - dim.y) / 2), active ? Vec4d(1.0, 1.0, 1.0, 1.0) : Vec4d(0.5, 0.5, 0.5, 1.0));
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
			// Signal auslösen
			check();
			changed(this);
		}
	}
}

void GUI_RadioButton::onMouseEnter(int buttons)
{
	mouseOver = true;
}

void GUI_RadioButton::onMouseLeave(int buttons)
{
	mouseOver = false;
}

void GUI_RadioButton::check()
{
	if(checked) return;

	checked = true;

	// alle anderen Radio-Buttons, die zur selben Gruppe gehören, ausschalten
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

	// Signal auslösen
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
		if(p_imageFilename) setImageFilename(p_imageFilename);

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