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
	dragOffset = 0;
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
		// draw the scroll bar background
		gui.renderFrame(Vec2i(0, 0), size, Vec2i(96, 96));

		if(!dir)
		{
			// draw the buttons
			gui.renderFrame(Vec2i(0, 0), Vec2i(size.x, size.x), pushedUp ? Vec2i(48, 96) : Vec2i(0, 96));
			gui.renderFrame(Vec2i(0, size.y - size.x), Vec2i(size.x, size.x), pushedDown ? Vec2i(48, 96) : Vec2i(0, 96));

			// draw the arrows
			Engine& engine = Engine::inst();
			int offset = (size.x - 16) / 2;
			engine.renderSprite(gui.getSkin(), Vec2i(offset, offset), pushedUp ? Vec2i(16, 224) : Vec2i(0, 224), Vec2i(16, 16), Vec4d(1.0), false, 0.0);
			engine.renderSprite(gui.getSkin(), Vec2i(offset, size.y - size.x + offset), pushedDown ? Vec2i(16, 224) : Vec2i(0, 224), Vec2i(16, 16), Vec4d(1.0), false, 180.0);

			// draw the drag bar
			gui.renderFrame(Vec2i(0, dragBarY), Vec2i(size.x, dragBarHeight), dragging ? Vec2i(48, 96) : Vec2i(0, 96));
		}
		else
		{
			// draw the buttons
			gui.renderFrame(Vec2i(0, 0), Vec2i(size.y, size.y), pushedUp ? Vec2i(48, 96) : Vec2i(0, 96));
			gui.renderFrame(Vec2i(size.x - size.y, 0), Vec2i(size.y, size.y), pushedDown ? Vec2i(48, 96) : Vec2i(0, 96));

			// draw the arrows
			Engine& engine = Engine::inst();
			int offset = (size.y - 16) / 2;
			engine.renderSprite(gui.getSkin(), Vec2i(offset, offset), pushedUp ? Vec2i(16, 224) : Vec2i(0, 224), Vec2i(16, 16), Vec4d(1.0), false, 270.0);
			engine.renderSprite(gui.getSkin(), Vec2i(size.x - size.y + offset, offset), pushedDown ? Vec2i(16, 224) : Vec2i(0, 224), Vec2i(16, 16), Vec4d(1.0), false, 90.0);

			// draw the drag bar
			gui.renderFrame(Vec2i(dragBarY, 0), Vec2i(dragBarHeight, size.y), dragging ? Vec2i(48, 96) : Vec2i(0, 96));
		}
	}
	else
	{
		// The same picture along either axis: a background, a button at each
		// end with an arrow pointing outward, and the drag bar between them.
		// Along the bar's axis a is the position and b the thickness; the
		// corner helper turns the two back into x and y.
		Renderer& renderer = Renderer::inst();
		const int thickness = dir ? size.y : size.x;
		const int length = dir ? size.x : size.y;
		const Vec4f frame(0.0f, 0.0f, 0.0f, 1.0f);

		gradient(0, length, Vec4f(0.4f, 0.4f, 0.4f, 1.0f), Vec4f(0.3f, 0.3f, 0.3f, 1.0f));
		gradient(0, thickness, pushedUp ? Vec4f(0.9f, 0.9f, 0.9f, 1.0f) : Vec4f(0.75f, 0.75f, 0.75f, 1.0f),
				 pushedUp ? Vec4f(0.8f, 0.8f, 0.8f, 1.0f) : Vec4f(0.65f, 0.65f, 0.65f, 1.0f));
		gradient(length - thickness, length, pushedDown ? Vec4f(0.9f, 0.9f, 0.9f, 1.0f) : Vec4f(0.75f, 0.75f, 0.75f, 1.0f),
				 pushedDown ? Vec4f(0.8f, 0.8f, 0.8f, 1.0f) : Vec4f(0.65f, 0.65f, 0.65f, 1.0f));

		// draw the arrows: a triangle in a unit box, scaled to the button and
		// mirrored for the far end
		const Vec4f dark(0.0f, 0.0f, 0.0f, 1.0f), grey(0.25f, 0.25f, 0.25f, 1.0f);
		const Vec2f upward[3] = {Vec2f(0.0f, -0.5f), Vec2f(0.5f, 0.5f), Vec2f(-0.5f, 0.5f)};
		const Vec2f leftward[3] = {Vec2f(-0.5f, 0.0f), Vec2f(0.5f, -0.5f), Vec2f(0.5f, 0.5f)};
		const Vec4f nearColors[3] = {dark, grey, grey};
		const Vec4f farColors[3] = {grey, dark, dark};
		renderer.push();
		renderer.translate(thickness / 2, thickness / 2);
		renderer.scale(0.5 * thickness, 0.5 * thickness);
		renderer.triangles(dir ? leftward : upward, nearColors, 3);
		renderer.pop();
		renderer.push();
		if(!dir)
		{
			renderer.translate(thickness / 2, length - thickness / 2);
			renderer.scale(0.5 * thickness, -0.5 * thickness);
		}
		else
		{
			renderer.translate(length - thickness / 2, thickness / 2);
			renderer.scale(-0.5 * thickness, 0.5 * thickness);
		}
		renderer.triangles(dir ? leftward : upward, farColors, 3);
		renderer.pop();

		// draw the frame and the lines that cut the buttons off
		renderer.hairlineRect(Vec2f(0.0f, 0.0f), static_cast<Vec2f>(size), frame);
		renderer.hairline(corner(thickness, 0), corner(thickness, thickness), frame);
		renderer.hairline(corner(length - thickness, 0), corner(length - thickness, thickness), frame);

		// draw the drag bar and its frame
		gradient(dragBarY, dragBarY + dragBarHeight,
				 dragging ? Vec4f(0.9f, 0.9f, 0.9f, 1.0f) : Vec4f(0.75f, 0.75f, 0.75f, 1.0f),
				 dragging ? Vec4f(0.8f, 0.8f, 0.8f, 1.0f) : Vec4f(0.65f, 0.65f, 0.65f, 1.0f));
		renderer.hairlineRect(corner(dragBarY, 0), corner(dragBarY + dragBarHeight, thickness), frame);
	}
}

Vec2f GUI_ScrollBar::corner(int along,
							 int across) const
{
	return static_cast<Vec2f>(dir ? Vec2i(along, across) : Vec2i(across, along));
}

void GUI_ScrollBar::gradient(int from,
							 int to,
							 const Vec4f& colorFrom,
							 const Vec4f& colorTo) const
{
	// The two colours run along the bar's axis, the first at `from`.
	const int thickness = dir ? size.y : size.x;
	const Vec2f corners[4] = {corner(from, 0), corner(from, thickness), corner(to, thickness), corner(to, 0)};
	const Vec4f colors[4] = {colorFrom, colorFrom, colorTo, colorTo};
	Renderer::inst().quad(corners, colors);
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

			// fire the scroll signal
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
			if(dragging) dragOffset = position.y - dragBarY;
			pushedUp = position.y < size.x;
			pushedDown = position.y >= size.y - size.x;
		}
		else
		{
			dragging = position.x >= static_cast<int>(dragBarY) &&
					   position.x < static_cast<int>(dragBarY + dragBarHeight);
			if(dragging) dragOffset = position.x - dragBarY;
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

			// fire the scroll signal
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
		// dragOffset is the point on the drag bar the hand took hold of, and
		// the cursor stays exactly there. Without it the drag bar would jump
		// under the hand on the first drag.
		if(!dir) setDragBarY(position.y - dragOffset);
		else setDragBarY(position.x - dragOffset);
	}
}

void GUI_ScrollBar::setScroll(int scroll)
{
	if(this->scroll == scroll) return;

	this->scroll = scroll;
	updateValues();

	updateReceiver();

	// fire the scroll signal
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

	// fire the scroll signal
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