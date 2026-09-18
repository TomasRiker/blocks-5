#include "pch.h"
#include "gui_editbox.h"
#include "gui_button.h"
#include "engine.h"

IMPL_CTOR(GUI_EditBox)
{
	cursor = selStart = selEnd = scroll = 0;
	text = "EditBox";
	p_submitButton = 0;
}

GUI_EditBox::~GUI_EditBox()
{
}

void GUI_EditBox::onRender()
{
	GUI& gui = GUI::inst();
	Renderer& renderer = Renderer::inst();
	bool focused = isFocused();

	if(useSkin())
	{
		// draw the edit box
		gui.renderFrame(Vec2i(0, 0), size, focused ? Vec2i(48, 144) : Vec2i(0, 144));
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

	// The text, the selection and the caret clipped to the inside of the
	// frame: the scope ends with the function.
	Renderer::ScissorScope clip(getAbsPosition() + Vec2i(2, 2), size - Vec2i(4, 4));

	Vec2i dim;
	std::vector<Vec2i> charPositions;
	p_font->measureText(text, &dim, &charPositions, Vec2i(4, 0));

	// work out the caret position
	Vec2i c = charPositions[cursor];

	// caret out of sight?
	int vcx = c.x - scroll;
	if(vcx < 16) scroll -= 16 - vcx;
	else if(vcx > size.x - 16) scroll += vcx - (size.x - 16);
	if(scroll < 0) scroll = 0;

	renderer.push();
	renderer.translate(static_cast<float>(-scroll), 0.0f);

	// write the text
	int py = (size.y - dim.y) / 2;
	p_font->renderText(text, Vec2i(4, py), active ? Vec4f(1.0f, 1.0f, 1.0f, 1.0f) : Vec4f(0.5f, 0.5f, 0.5f, 1.0f));

	if(focused)
	{
		if(selStart != selEnd)
		{
			// draw the selection
			const Vec4f top(0.25f, 0.25f, 1.0f, 0.5f), bottom(0.2f, 0.2f, 0.8f, 0.5f);
			const Vec4f colors[4] = {top, top, bottom, bottom};
			int h = p_font->getLineHeight();
			for(uint i = selStart; i < selEnd; i++)
			{
				const Vec2i p = charPositions[i];
				int w = max(0, charPositions[i + 1].x - p.x);
				const Vec2f lo = static_cast<Vec2f>(p + Vec2i(0, py + 1)), hi = static_cast<Vec2f>(p + Vec2i(w, py + 1 + h));
				const Vec2f corners[4] = {lo, Vec2f(hi.x, lo.y), hi, Vec2f(lo.x, hi.y)};
				renderer.quad(corners, colors);
			}
		}

		// draw the caret
		const float alpha = 0.6f + 0.4f * sinf(0.02f * Engine::inst().getTime());
		renderer.hairline(static_cast<Vec2f>(c + Vec2i(0, py + 1)), static_cast<Vec2f>(c + Vec2i(0, py + 1 + p_font->getLineHeight())),
						  Vec4f(1.0f, 1.0f, 1.0f, alpha));
	}

	renderer.pop();
}

void GUI_EditBox::setText(const std::string& text)
{
	this->text = text;
	cursor = scroll = 0;
	selStart = selEnd = 0;

	// fire the signal
	changed(this);
}

void GUI_EditBox::onMouseDown(const Vec2i& position,
							  int buttons)
{
	if(buttons & 1)
	{
		bool shift = Engine::inst().isKeyDown(SDLK_LSHIFT) || Engine::inst().isKeyDown(SDLK_RSHIFT);
		setCursor(getIndexAt(position), shift);
	}
}

void GUI_EditBox::onMouseMove(const Vec2i& position,
							  const Vec2i& movement,
							  int buttons)
{
	if(GUI::inst().getMouseDownElement() == this && (buttons & 1)) setCursor(getIndexAt(position), true);
}

void GUI_EditBox::onKeyEvent(const SDL_KeyboardEvent& event)
{
	// Only a key press matters here.
	if(event.type != SDL_KEYDOWN) return;

	// Shift pressed?
	bool shift = (event.keysym.mod & KMOD_LSHIFT) || (event.keysym.mod & KMOD_RSHIFT);

	// Ctrl pressed?
	bool ctrl = (event.keysym.mod & KMOD_LCTRL) || (event.keysym.mod & KMOD_RCTRL);

	switch(event.keysym.sym)
	{
	case SDLK_TAB:
		// forward the event to the parent element
		if(p_parent) p_parent->onKeyEvent(event);
		break;
	case SDLK_LEFT:
		setCursor(cursor ? cursor - 1 : cursor, shift);
		break;
	case SDLK_RIGHT:
		setCursor(cursor < static_cast<uint>(text.length()) ? cursor + 1 : cursor, shift);
		break;
	case SDLK_HOME:
		setCursor(0, shift);
		break;
	case SDLK_END:
		setCursor(static_cast<uint>(text.length()), shift);
		break;
	case SDLK_DELETE:
		if(active) del();
		break;
	case SDLK_BACKSPACE:
		if(active) backspace();
		break;
	case SDLK_RETURN:
	case SDLK_KP_ENTER:
		// With no button of its own for it, Return belongs to the dialog, where
		// it means OK - otherwise nothing would ever arrive there from inside
		// an edit box. The button only on a fresh press, though - it is a
		// command.
		if(active && p_submitButton) { if(!GUI::inst().isKeyRepeat()) p_submitButton->click(); }
		else if(p_parent) p_parent->onKeyEvent(event);
		break;
	case SDLK_ESCAPE:
		// Escape is never an input. In the default branch it would be dropped
		// silently by the unicode < 32 test, and the dialog behind would never
		// see it.
		if(p_parent) p_parent->onKeyEvent(event);
		break;
	case SDLK_a:
	case SDLK_c:
	case SDLK_v:
	case SDLK_x:
		if(ctrl)
		{
			if(event.keysym.sym == SDLK_a)
			{
				// select all
				cursor = selStart = selEnd = 0;
				setCursor(static_cast<uint>(text.length()), true);
			}
			else if(event.keysym.sym == SDLK_c ||
					event.keysym.sym == SDLK_x)
			{
				// copy/cut
				if(selStart != selEnd)
				{
					GUI::inst().setClipboard(std::string(text.begin() + selStart, text.begin() + selEnd));
					if(active && event.keysym.sym == SDLK_x) replaceSelection("");
				}
			}
			else if(active && event.keysym.sym == SDLK_v)
			{
				// paste
				const std::string& clipboard = GUI::inst().getClipboard();
				if(!clipboard.empty()) replaceSelection(clipboard);
			}
			// Handled, and the letter must not go on to be typed as well.
			// What unicode a Ctrl combination carries is the platform's
			// choice - under X11 it is the letter itself, measured: Ctrl+A
			// selected everything and put an "a" in its place - so that
			// cannot be left to the unicode test below.
			break;
		}
		// Without Ctrl the letter is text like any other.
	default:
		{
			char c = static_cast<char>(event.keysym.unicode);
			if(active && (c >= 32 || c < 0)) replaceSelection(std::string("") + c);
			break;
		}
	}
}

void GUI_EditBox::onTabbedIn()
{
	// select all
	cursor = selStart = selEnd = 0;
	setCursor(static_cast<uint>(text.length()), true);
}

void GUI_EditBox::replaceSelection(const std::string& replacement)
{
	if(selStart == selEnd)
	{
		const std::string textBeforeCursor(text.begin(), text.begin() + cursor);
		const std::string textAfterCursor(text.begin() + cursor, text.end());
		text = textBeforeCursor + replacement + textAfterCursor;
		cursor = static_cast<uint>(textBeforeCursor.length() + replacement.length());
	}
	else
	{
		const std::string textBeforeSelection(text.begin(), text.begin() + selStart);
		const std::string textAfterSelection(text.begin() + selEnd, text.end());
		text = textBeforeSelection + replacement + textAfterSelection;
		cursor = static_cast<uint>(textBeforeSelection.length() + replacement.length());
		selStart = selEnd = 0;
	}

	// fire the signal
	changed(this);
}

void GUI_EditBox::del()
{
	if(selStart == selEnd)
	{
		if(cursor < static_cast<uint>(text.length()))
		{
			const std::string textBeforeCursor(text.begin(), text.begin() + cursor);
			const std::string textAfterCursor(text.begin() + cursor + 1, text.end());
			text = textBeforeCursor + textAfterCursor;
			cursor = static_cast<uint>(textBeforeCursor.length());

			// fire the signal
			changed(this);
		}
	}
	else
	{
		replaceSelection("");
	}
}

void GUI_EditBox::backspace()
{
	if(selStart == selEnd)
	{
		if(cursor)
		{
			const std::string textBeforeCursor(text.begin(), text.begin() + cursor - 1);
			const std::string textAfterCursor(text.begin() + cursor, text.end());
			text = textBeforeCursor + textAfterCursor;
			cursor = static_cast<uint>(textBeforeCursor.length());

			// fire the signal
			changed(this);
		}
	}
	else
	{
		replaceSelection("");
	}
}

void GUI_EditBox::setCursor(uint cursor,
							bool shift)
{
	if(cursor > static_cast<uint>(text.length())) cursor = static_cast<uint>(text.length());

	if(shift)
	{
		if(selStart == selEnd)
		{
			// No selection yet.
			if(cursor > this->cursor) selStart = this->cursor, selEnd = cursor;
			else if(cursor < this->cursor) selStart = cursor, selEnd = this->cursor;
		}
		else
		{
			if(this->cursor == selStart)
			{
				if(cursor <= selEnd) selStart = cursor;
				else
				{
					selStart = selEnd;
					selEnd = cursor;
				}
			}
			else if(this->cursor == selEnd)
			{
				if(cursor >= selStart) selEnd = cursor;
				else
				{
					selEnd = selStart;
					selStart = cursor;
				}
			}
		}
	}
	else
	{
		// clear the selection
		selStart = selEnd = 0;
	}

	this->cursor = cursor;
}

uint GUI_EditBox::getIndexAt(const Vec2i& position)
{
	// The characters where they are drawn, so that the rule reads the same
	// as the multi-line box's: a click within the first two pixels of a
	// character lands before it, from the third after it.
	std::vector<Vec2i> charPositions;
	p_font->measureText(text, 0, &charPositions, Vec2i(4, 0));

	uint i;
	for(i = 0; i < static_cast<uint>(text.length()); i++)
	{
		if(charPositions[i].x + 2 >= position.x + scroll) break;
	}

	return i;
}

void GUI_EditBox::readAttributes(TiXmlElement* p_element)
{
	TiXmlElement* e = p_element->FirstChildElement("Text");
	if(e)
	{
		const char* p_text = e->GetText();
		setText(p_text ? p_text : "");
	}

	e = p_element->FirstChildElement("SubmitButton");
	if(e)
	{
		GUI_Button* p_button = static_cast<GUI_Button*>(p_parent->getChild(e->GetText()));
		setSubmitButton(p_button);
	}
}