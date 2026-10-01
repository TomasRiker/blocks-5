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
		// draw the title bar
		gui.renderFrame(Vec2i(0, -4), Vec2i(size.x, 24), front ? Vec2i(48, 0) : Vec2i(0, 0));

		// draw the background
		gui.renderFrame(Vec2i(0, 20), Vec2i(size.x, size.y - 20), front ? Vec2i(48, 48) : Vec2i(0, 48));

		offset = -4;
	}
	else
	{
		// draw the background and the title bar
		Renderer& renderer = Renderer::inst();
		const Vec2f s = static_cast<Vec2f>(size);
		const Vec2f body[4] = {Vec2f(0.0f, 20.0f), Vec2f(s.x, 20.0f), s, Vec2f(0.0f, s.y)};
		const Vec4f bodyTop(0.65f, 0.65f, 0.65f, 1.0f), bodyBottom(0.55f, 0.55f, 0.55f, 1.0f);
		const Vec4f bodyColors[4] = {bodyTop, bodyTop, bodyBottom, bodyBottom};
		renderer.quad(body, bodyColors);
		const Vec2f bar[4] = {Vec2f(0.0f, 0.0f), Vec2f(s.x, 0.0f), Vec2f(s.x, 20.0f), Vec2f(0.0f, 20.0f)};
		const Vec4f barTop = front ? Vec4f(0.5f, 0.5f, 1.0f, 1.0f) : Vec4f(0.4f, 0.4f, 0.7f, 1.0f);
		const Vec4f barBottom = front ? Vec4f(0.35f, 0.35f, 1.0f, 1.0f) : Vec4f(0.3f, 0.3f, 0.7f, 1.0f);
		const Vec4f barColors[4] = {barTop, barTop, barBottom, barBottom};
		renderer.quad(bar, barColors);

		// draw the frame
		const Vec4f white(1.0f, 1.0f, 1.0f, 1.0f);
		renderer.hairlineRect(Vec2f(0.0f, 0.0f), s, white);
		renderer.hairline(Vec2f(0.0f, 20.0f), Vec2f(s.x, 20.0f), white);
	}

	// write the title
	Vec2i dim;
	std::string shownTitle = localizeString(this->title);
	p_font->measureText(shownTitle, &dim, 0);
	p_font->renderText(shownTitle, Vec2i((size.x - dim.x) / 2, 3 + offset), Vec4f(1.0f, 1.0f, 1.0f, 1.0f));
}

bool GUI_Window::getClipRect(Vec2i* p_position,
							 Vec2i* p_size) const
{
	// The area under the title bar, one pixel in from the frame.
	*p_position = Vec2i(1, 20);
	*p_size = Vec2i(size.x - 1, size.y - 21);
	return true;
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
	// Only once the button is up. A quick drag leaves the window behind for a
	// tick: GUI::update() dispatches the leave before the move the window
	// follows, so ending the drag here would drop the window mid-gesture. The
	// moves keep arriving meanwhile, since GUI::update() delivers them to the
	// element the button went down on as well as to the one under the cursor.
	if(!(buttons & 1)) moving = false;
}

void GUI_Window::onMouseMove(const Vec2i& position,
							 const Vec2i& movement,
							 int buttons)
{
	// The button has to still be held. A release the game never saw - the mouse
	// let go outside its own window, where SDL may not report it - would
	// otherwise leave the window stuck to the cursor for good.
	if(!(buttons & 1))
	{
		moving = false;
		return;
	}

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