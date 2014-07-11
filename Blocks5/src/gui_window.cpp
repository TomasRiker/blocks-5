#include "pch.h"
#include "gui_window.h"

IMPL_CTOR(GUI_Window)
{
	title = "Window";
	moving = false;
	p_oldFocusElement = 0;
}

GUI_Window::~GUI_Window()
{
}

void GUI_Window::onRender()
{
	GUI& gui = GUI::inst();
	bool front = isFocusedIndirectly();
	int offset = 0;

	if(useSkin())
	{
		// Titelleiste zeichnen
		gui.renderFrame(Vec2i(0, -4), Vec2i(size.x, 24), front ? Vec2i(48, 0) : Vec2i(0, 0));

		// Hintergrund zeichnen
		gui.renderFrame(Vec2i(0, 20), Vec2i(size.x, size.y - 20), front ? Vec2i(48, 48) : Vec2i(0, 48));

		offset = -4;
	}
	else
	{
		// Hintergrund und Titelleiste zeichnen
		glBegin(GL_QUADS);
		glColor4d(0.65, 0.65, 0.65, 1.0);
		glVertex2i(0, 20);
		glVertex2i(size.x, 20);
		glColor4d(0.55, 0.55, 0.55, 1.0);
		glVertex2i(size.x, size.y);
		glVertex2i(0, size.y);
		if(front) glColor4d(0.5, 0.5, 1.0, 1.0);
		else glColor4d(0.4, 0.4, 0.7, 1.0);
		glVertex2i(0, 0);
		glVertex2i(size.x, 0);
		if(front) glColor4d(0.35, 0.35, 1.0, 1.0);
		else glColor4d(0.3, 0.3, 0.7, 1.0);
		glVertex2i(size.x, 20);
		glVertex2i(0, 20);
		glEnd();

		// Rahmen zeichnen
		glColor4d(1.0, 1.0, 1.0, 1.0);
		glBegin(GL_LINE_LOOP);
		glVertex2i(0, 0);
		glVertex2i(size.x, 0);
		glVertex2i(size.x, size.y);
		glVertex2i(0, size.y);
		glEnd();
		glBegin(GL_LINES);
		glVertex2i(0, 20);
		glVertex2i(size.x, 20);
		glEnd();
	}

	// Titel schreiben
	Vec2i dim;
	std::string title = localizeString(this->title);
	p_font->measureText(title, &dim, 0);
	p_font->renderText(localizeString(title), Vec2i((size.x - dim.x) / 2, 3 + offset), Vec4d(1.0, 1.0, 1.0, 1.0));

	const Vec2i pos = getAbsPosition();
	glEnable(GL_SCISSOR_TEST);
	int h = GUI::inst().getRoot()->getSize().y;
	glScissor(pos.x + 1, h - pos.y - size.y + 1, size.x - 1, size.y - 21);
}

void GUI_Window::onRenderEnd()
{
	glDisable(GL_SCISSOR_TEST);
}

void GUI_Window::onMouseDown(const Vec2i& position,
							 int buttons)
{
	if(buttons & 1)
	{
		moving = position.y <= 20;
		if(moving)
		{
			p_oldFocusElement = GUI::inst().getOldFocusElement();
		}
	}
}

void GUI_Window::onMouseUp(const Vec2i& position,
						   int buttons)
{
	if(buttons & 1)
	{
		if(moving)
		{
			moving = false;
			if(p_oldFocusElement)
			{
				if(p_oldFocusElement->isChildOf(this))
				{
					GUI::inst().setFocusElement(p_oldFocusElement);
				}
			}
		}
	}
}

void GUI_Window::onMouseLeave(int buttons)
{
	moving = false;
}

void GUI_Window::onMouseMove(const Vec2i& position,
							 const Vec2i& movement,
							 int buttons)
{
	if(moving) setPosition(getPosition() + movement);
}

void GUI_Window::readAttributes(TiXmlElement* p_element)
{
	TiXmlElement* e = p_element->FirstChildElement("Title");
	if(e)
	{
		const char* p_title = e->GetText();
		setTitle(p_title ? p_title : "");
	}
}