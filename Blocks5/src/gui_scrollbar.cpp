#include "pch.h"
#include "gui_scrollbar.h"
#include "gui_statictext.h"
#include "engine.h"

IMPL_CTOR(GUI_ScrollBar)
{
	dir = size.x > size.y;
	scroll = 0;
	areaSize = 100;
	pageSize = 10;
	dragging = pushedUp = pushedDown = false;
	pushTime = 0;
	p_receiver = 0;
	updateValues();
}

GUI_ScrollBar::~GUI_ScrollBar()
{
}

void GUI_ScrollBar::onRender()
{
	GUI& gui = GUI::inst();

	if(useSkin())
	{
		// Scroll-Bar-Hintergrund zeichnen
		gui.renderFrame(Vec2i(0, 0), size, Vec2i(96, 96));

		if(!dir)
		{
			// Buttons zeichnen
			gui.renderFrame(Vec2i(0, 0), Vec2i(size.x, size.x), pushedUp ? Vec2i(48, 96) : Vec2i(0, 96));
			gui.renderFrame(Vec2i(0, size.y - size.x), Vec2i(size.x, size.x), pushedDown ? Vec2i(48, 96) : Vec2i(0, 96));

			// Pfeile zeichnen
			Engine& engine = Engine::inst();
			int offset = (size.x - 16) / 2;
			engine.renderSprite(gui.getSkin(), Vec2i(offset, offset), pushedUp ? Vec2i(16, 224) : Vec2i(0, 224), Vec2i(16, 16), Vec4d(1.0), false, 0.0);
			engine.renderSprite(gui.getSkin(), Vec2i(offset, size.y - size.x + offset), pushedDown ? Vec2i(16, 224) : Vec2i(0, 224), Vec2i(16, 16), Vec4d(1.0), false, 180.0);

			// Drag-Bar zeichnen
			gui.renderFrame(Vec2i(0, dragBarY), Vec2i(size.x, dragBarHeight), dragging ? Vec2i(48, 96) : Vec2i(0, 96));
		}
		else
		{
			// Buttons zeichnen
			gui.renderFrame(Vec2i(0, 0), Vec2i(size.y, size.y), pushedUp ? Vec2i(48, 96) : Vec2i(0, 96));
			gui.renderFrame(Vec2i(size.x - size.y, 0), Vec2i(size.y, size.y), pushedDown ? Vec2i(48, 96) : Vec2i(0, 96));

			// Pfeile zeichnen
			Engine& engine = Engine::inst();
			int offset = (size.y - 16) / 2;
			engine.renderSprite(gui.getSkin(), Vec2i(offset, offset), pushedUp ? Vec2i(16, 224) : Vec2i(0, 224), Vec2i(16, 16), Vec4d(1.0), false, 270.0);
			engine.renderSprite(gui.getSkin(), Vec2i(size.x - size.y + offset, offset), pushedDown ? Vec2i(16, 224) : Vec2i(0, 224), Vec2i(16, 16), Vec4d(1.0), false, 90.0);

			// Drag-Bar zeichnen
			gui.renderFrame(Vec2i(dragBarY, 0), Vec2i(dragBarHeight, size.y), dragging ? Vec2i(48, 96) : Vec2i(0, 96));
		}
	}
	else
	{
		if(!dir)
		{
			// Scroll-Bar-Hintergrund und Button-Hintergrund zeichnen
			glBegin(GL_QUADS);

			glColor4d(0.4, 0.4, 0.4, 1.0);
			glVertex2i(0, 0);
			glVertex2i(size.x, 0);
			glColor4d(0.3, 0.3, 0.3, 1.0);
			glVertex2i(size.x, size.y);
			glVertex2i(0, size.y);

			if(pushedUp) glColor4d(0.9, 0.9, 0.9, 1.0);
			else glColor4d(0.75, 0.75, 0.75, 1.0);
			glVertex2i(0, 0);
			glVertex2i(size.x, 0);
			if(pushedUp) glColor4d(0.8, 0.8, 0.8, 1.0);
			else glColor4d(0.65, 0.65, 0.65, 1.0);
			glVertex2i(size.x, size.x);
			glVertex2i(0, size.x);

			if(pushedDown) glColor4d(0.9, 0.9, 0.9, 1.0);
			else glColor4d(0.75, 0.75, 0.75, 1.0);
			glVertex2i(0, size.y - size.x);
			glVertex2i(size.x, size.y - size.x);
			if(pushedDown) glColor4d(0.8, 0.8, 0.8, 1.0);
			else glColor4d(0.65, 0.65, 0.65, 1.0);
			glVertex2i(size.x, size.y);
			glVertex2i(0, size.y);

			glEnd();

			// Pfeile zeichnen
			glPushMatrix();
			glTranslated(size.x / 2, size.x / 2, 0.0);
			glScaled(0.5 * size.x, 0.5 * size.x, 1.0);
			glBegin(GL_TRIANGLES);
			glColor4d(0.0, 0.0, 0.0, 1.0);
			glVertex2d(0.0, -0.5);
			glColor4d(0.25, 0.25, 0.25, 1.0);
			glVertex2d(0.5, 0.5);
			glVertex2d(-0.5, 0.5);
			glEnd();
			glPopMatrix();
			glPushMatrix();
			glTranslated(size.x / 2, size.y - size.x / 2, 0.0);
			glScaled(0.5 * size.x, -0.5 * size.x, 1.0);
			glBegin(GL_TRIANGLES);
			glColor4d(0.25, 0.25, 0.25, 1.0);
			glVertex2d(0.0, -0.5);
			glColor4d(0.0, 0.0, 0.0, 1.0);
			glVertex2d(0.5, 0.5);
			glVertex2d(-0.5, 0.5);
			glEnd();
			glPopMatrix();

			// Rahmen zeichnen
			glColor4d(0.0, 0.0, 0.0, 1.0);
			glBegin(GL_LINE_LOOP);
			glVertex2i(0, 0);
			glVertex2i(size.x, 0);
			glVertex2i(size.x, size.y);
			glVertex2i(0, size.y);
			glEnd();
			glBegin(GL_LINES);
			glVertex2i(0, size.x);
			glVertex2i(size.x, size.x);
			glVertex2i(0, size.y - size.x);
			glVertex2i(size.x, size.y - size.x);
			glEnd();

			// Drag-Bar-Hintergrund zeichnen
			glBegin(GL_QUADS);
			if(dragging) glColor4d(0.9, 0.9, 0.9, 1.0);
			else glColor4d(0.75, 0.75, 0.75, 1.0);
			glVertex2i(0, dragBarY);
			glVertex2i(size.x, dragBarY);
			if(dragging) glColor4d(0.8, 0.8, 0.8, 1.0);
			else glColor4d(0.65, 0.65, 0.65, 1.0);
			glVertex2i(size.x, dragBarY + dragBarHeight);
			glVertex2i(0, dragBarY + dragBarHeight);
			glEnd();

			// Rahmen zeichnen
			glColor4d(0.0, 0.0, 0.0, 1.0);
			glBegin(GL_LINE_LOOP);
			glVertex2i(0, dragBarY);
			glVertex2i(size.x, dragBarY);
			glVertex2i(size.x, dragBarY + dragBarHeight);
			glVertex2i(0, dragBarY + dragBarHeight);
			glEnd();
		}
		else
		{
			// Scroll-Bar-Hintergrund und Button-Hintergrund zeichnen
			glBegin(GL_QUADS);

			glColor4d(0.4, 0.4, 0.4, 1.0);
			glVertex2i(0, 0);
			glVertex2i(size.x, 0);
			glColor4d(0.3, 0.3, 0.3, 1.0);
			glVertex2i(size.x, size.y);
			glVertex2i(0, size.y);

			if(pushedUp) glColor4d(0.9, 0.9, 0.9, 1.0);
			else glColor4d(0.75, 0.75, 0.75, 1.0);
			glVertex2i(0, 0);
			glVertex2i(size.y, 0);
			if(pushedUp) glColor4d(0.8, 0.8, 0.8, 1.0);
			else glColor4d(0.65, 0.65, 0.65, 1.0);
			glVertex2i(size.y, size.y);
			glVertex2i(0, size.y);

			if(pushedDown) glColor4d(0.9, 0.9, 0.9, 1.0);
			else glColor4d(0.75, 0.75, 0.75, 1.0);
			glVertex2i(size.x - size.y, 0);
			glVertex2i(size.x, 0);
			if(pushedDown) glColor4d(0.8, 0.8, 0.8, 1.0);
			else glColor4d(0.65, 0.65, 0.65, 1.0);
			glVertex2i(size.x, size.y);
			glVertex2i(size.x - size.y, size.y);

			glEnd();

			// Pfeile zeichnen
			glPushMatrix();
			glTranslated(size.y / 2, size.y / 2, 0.0);
			glScaled(0.5 * size.y, 0.5 * size.y, 1.0);
			glBegin(GL_TRIANGLES);
			glColor4d(0.0, 0.0, 0.0, 1.0);
			glVertex2d(-0.5, 0.0);
			glColor4d(0.25, 0.25, 0.25, 1.0);
			glVertex2d(0.5, -0.5);
			glVertex2d(0.5, 0.5);
			glEnd();
			glPopMatrix();
			glPushMatrix();
			glTranslated(size.x - size.y / 2, size.y / 2, 0.0);
			glScaled(0.5 * size.y, -0.5 * size.y, 1.0);
			glBegin(GL_TRIANGLES);
			glColor4d(0.25, 0.25, 0.25, 1.0);
			glVertex2d(0.5, 0.0);
			glColor4d(0.0, 0.0, 0.0, 1.0);
			glVertex2d(-0.5, 0.5);
			glVertex2d(-0.5, -0.5);
			glEnd();
			glPopMatrix();

			// Rahmen zeichnen
			glColor4d(0.0, 0.0, 0.0, 1.0);
			glBegin(GL_LINE_LOOP);
			glVertex2i(0, 0);
			glVertex2i(size.x, 0);
			glVertex2i(size.x, size.y);
			glVertex2i(0, size.y);
			glEnd();
			glBegin(GL_LINES);
			glVertex2i(size.y, 0);
			glVertex2i(size.y, size.y);
			glVertex2i(size.x - size.y, 0);
			glVertex2i(size.x - size.y, size.y);
			glEnd();

			// Drag-Bar-Hintergrund zeichnen
			glBegin(GL_QUADS);
			if(dragging) glColor4d(0.9, 0.9, 0.9, 1.0);
			else glColor4d(0.75, 0.75, 0.75, 1.0);
			glVertex2i(dragBarY, 0);
			glVertex2i(dragBarY, size.y);
			if(dragging) glColor4d(0.8, 0.8, 0.8, 1.0);
			else glColor4d(0.65, 0.65, 0.65, 1.0);
			glVertex2i(dragBarY + dragBarHeight, size.y);
			glVertex2i(dragBarY + dragBarHeight, 0);
			glEnd();

			// Rahmen zeichnen
			glColor4d(0.0, 0.0, 0.0, 1.0);
			glBegin(GL_LINE_LOOP);
			glVertex2i(dragBarY, 0);
			glVertex2i(dragBarY, size.y);
			glVertex2i(dragBarY + dragBarHeight, size.y);
			glVertex2i(dragBarY + dragBarHeight, 0);
			glEnd();
		}
	}
}

void GUI_ScrollBar::onUpdate()
{
	if(pushedUp || pushedDown)
	{
		pushTime -= 5;
		if(pushTime <= 0)
		{
			if(pushedUp) scroll -= 10;
			else scroll += 10;
			updateValues();
			pushTime = 20;

			updateReceiver();

			// Scroll-Signal auslösen
			changed(this);
		}
	}
}

void GUI_ScrollBar::onMouseDown(const Vec2i& position,
								int buttons)
{
	if(buttons & 1)
	{
		if(!dir)
		{
			dragging = position.y >= static_cast<int>(dragBarY) &&
					   position.y < static_cast<int>(dragBarY + dragBarHeight);
			if(dragging) dragStartY = position.x;
			pushedUp = position.y < size.x;
			pushedDown = position.y >= size.y - size.x;
		}
		else
		{
			dragging = position.x >= static_cast<int>(dragBarY) &&
					   position.x < static_cast<int>(dragBarY + dragBarHeight);
			if(dragging) dragStartY = position.y;
			pushedUp = position.x < size.y;
			pushedDown = position.x >= size.x - size.y;
		}

		if(pushedUp || pushedDown)
		{
			if(pushedUp) scroll -= 10;
			else scroll += 10;
			updateValues();
			pushTime = 40;

			updateReceiver();

			// Scroll-Signal auslösen
			changed(this);
		}
		else if(!dragging)
		{
			if(!dir) setDragBarY(position.y);
			else setDragBarY(position.x);
		}
	}
}

void GUI_ScrollBar::onMouseUp(const Vec2i& position,
							  int buttons)
{
	if(buttons & 1)
	{
		dragging = pushedUp = pushedDown = false;
	}
}

void GUI_ScrollBar::onMouseMove(const Vec2i& position,
								const Vec2i& movement,
								int buttons)
{
	if(dragging)
	{
		if(!dir) setDragBarY(position.y - dragStartY);
		else setDragBarY(position.x - dragStartY);
	}
}

void GUI_ScrollBar::setScroll(int scroll)
{
	if(this->scroll == scroll) return;

	this->scroll = scroll;
	updateValues();

	updateReceiver();

	// Scroll-Signal auslösen
	changed(this);
}

void GUI_ScrollBar::setAreaSize(int areaSize)
{
	if(this->areaSize == areaSize) return;

	this->areaSize = areaSize;
	updateValues();
}

void GUI_ScrollBar::setPageSize(int pageSize)
{
	if(this->pageSize == pageSize) return;

	this->pageSize = pageSize;
	updateValues();
}

void GUI_ScrollBar::setReceiver(GUI_StaticText* p_receiver)
{
	if(this->p_receiver == p_receiver) return;

	this->p_receiver = p_receiver;
	updateReceiver();
}

void GUI_ScrollBar::updateValues()
{
	if(pageSize >= areaSize)
	{
		scroll = 0;

		if(!dir)
		{
			dragBarY = size.x;
			dragBarHeight = size.y - 2 * size.x;
		}
		else
		{
			dragBarY = size.y;
			dragBarHeight = size.x - 2 * size.y;
		}
	}
	else
	{
		scroll = clamp(scroll, 0, areaSize - pageSize);

		if(!dir)
		{
			dragBarHeight = static_cast<int>(static_cast<double>(pageSize) / areaSize * (size.y - 2 * size.x));
			dragBarY = size.x + static_cast<int>(static_cast<double>(scroll) / (areaSize - pageSize) * (size.y - 2 * size.x - dragBarHeight));
		}
		else
		{
			dragBarHeight = static_cast<int>(static_cast<double>(pageSize) / areaSize * (size.x - 2 * size.y));
			dragBarY = size.y + static_cast<int>(static_cast<double>(scroll) / (areaSize - pageSize) * (size.x - 2 * size.y - dragBarHeight));
		}
	}
}

void GUI_ScrollBar::setDragBarY(int dragBarY)
{
	if(!dir)
	{
		this->dragBarY = clamp(dragBarY, size.x, size.y - size.x - dragBarHeight);
		scroll = static_cast<int>(static_cast<double>((areaSize - pageSize) * (size.x - this->dragBarY)) / (dragBarHeight + 2 * size.x - size.y));
	}
	else
	{
		this->dragBarY = clamp(dragBarY, size.y, size.x - size.y - dragBarHeight);
		scroll = static_cast<int>(static_cast<double>((areaSize - pageSize) * (size.y - this->dragBarY)) / (dragBarHeight + 2 * size.y - size.x));
	}

	scroll = clamp(scroll, 0, areaSize - pageSize);

	updateReceiver();

	// Scroll-Signal auslösen
	changed(this);
}

void GUI_ScrollBar::updateReceiver()
{
	if(p_receiver)
	{
		char s[32]; sprintf(s, "%d", scroll);
		p_receiver->setText(s);
	}
}

void GUI_ScrollBar::readAttributes(TiXmlElement* p_element)
{
	TiXmlElement* e = p_element->FirstChildElement("AreaSize");
	if(e)
	{
		int areaSize = 0;
		sscanf(e->GetText(), "%d", &areaSize);
		setAreaSize(areaSize);
	}

	e = p_element->FirstChildElement("PageSize");
	if(e)
	{
		int pageSize = 0;
		sscanf(e->GetText(), "%d", &pageSize);
		setPageSize(pageSize);
	}

	e = p_element->FirstChildElement("Receiver");
	if(e)
	{
		GUI_StaticText* p_receiver = static_cast<GUI_StaticText*>(p_parent->getChild(e->GetText()));
		setReceiver(p_receiver);
	}
}