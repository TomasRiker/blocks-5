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
	bool focused = isFocused();

	if(useSkin())
	{
		// Eingabefeld zeichnen
		gui.renderFrame(Vec2i(0, 0), size, focused ? Vec2i(48, 144) : Vec2i(0, 144));
	}
	else
	{
		// Hintergrund zeichnen
		glBegin(GL_QUADS);
		if(focused) glColor4d(0.6, 0.6, 0.6, 1.0);
		else glColor4d(0.4, 0.4, 0.4, 1.0);
		glVertex2i(0, 0);
		glVertex2i(size.x, 0);
		if(focused) glColor4d(0.5, 0.5, 0.5, 1.0);
		else glColor4d(0.3, 0.3, 0.3, 1.0);
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

	const Vec2i pos = getAbsPosition();
	glEnable(GL_SCISSOR_TEST);
	int h = gui.getRoot()->getSize().y;
	glScissor(pos.x + 2, h - pos.y - size.y + 2, size.x - 4, size.y - 4);

	Vec2i dim;
	std::vector<Vec2i> charPositions;
	p_font->measureText(text, &dim, &charPositions, Vec2i(4, 0));

	// Position des Cursors berechnen
	Vec2i c = charPositions[cursor];

	// Cursor unsichtbar?
	int vcx = c.x - scroll;
	if(vcx < 16) scroll -= 16 - vcx;
	else if(vcx > size.x - 16) scroll += vcx - (size.x - 16);
	if(scroll < 0) scroll = 0;

	glPushMatrix();
	glTranslated(-scroll, 0.0, 0.0);

	// Text schreiben
	int py = (size.y - dim.y) / 2;
	p_font->renderText(text, Vec2i(4, py), active ? Vec4d(1.0, 1.0, 1.0, 1.0) : Vec4d(0.5, 0.5, 0.5, 1.0));

	if(focused)
	{
		if(selStart != selEnd)
		{
			// Auswahl zeichnen
			glBegin(GL_QUADS);

			int h = p_font->getLineHeight();
			for(uint i = selStart; i < selEnd; i++)
			{
				const Vec2i p = charPositions[i];
				int w = max(0, charPositions[i + 1].x - p.x);
				glColor4d(0.25, 0.25, 1.0, 0.5);
				glVertex2i(p.x, p.y + py + 1);
				glVertex2i(p.x + w, p.y + py + 1);
				glColor4d(0.2, 0.2, 0.8, 0.5);
				glVertex2i(p.x + w, p.y + py + 1 + h);
				glVertex2i(p.x, p.y + py + 1 + h);
			}

			glEnd();
		}

		// Cursor zeichnen
		glBegin(GL_LINES);
		double alpha = 0.6 + 0.4 * sin(0.02 * Engine::inst().getTime());
		glColor4d(1.0, 1.0, 1.0, alpha);
		glVertex2i(c.x, c.y + py + 1);
		glVertex2i(c.x, c.y + py + 1 + p_font->getLineHeight());
		glEnd();
	}

	glPopMatrix();
	glDisable(GL_SCISSOR_TEST);
}

void GUI_EditBox::setText(const std::string& text)
{
	this->text = text;
	cursor = scroll = 0;
	selStart = selEnd = 0;

	// Signal auslösen
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
	// Uns interessiert nur, ob eine Taste gedrückt wurde.
	if(event.type != SDL_KEYDOWN) return;

	// Shift gedrückt?
	bool shift = (event.keysym.mod & KMOD_LSHIFT) || (event.keysym.mod & KMOD_RSHIFT);

	// Strg gedrückt?
	bool ctrl = (event.keysym.mod & KMOD_LCTRL) || (event.keysym.mod & KMOD_RCTRL);

	switch(event.keysym.sym)
	{
	case SDLK_TAB:
		// Ereignis an das Elternelement weiterleiten
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
		if(active && p_submitButton) p_submitButton->click();
		break;
	case SDLK_a:
	case SDLK_c:
	case SDLK_v:
	case SDLK_x:
		if(ctrl)
		{
			if(event.keysym.sym == SDLK_a)
			{
				// alles auswählen
				cursor = selStart = selEnd = 0;
				setCursor(static_cast<uint>(text.length()), true);
			}
			else if(event.keysym.sym == SDLK_c ||
					event.keysym.sym == SDLK_x)
			{
				// kopieren/ausschneiden
				if(selStart != selEnd)
				{
					GUI::inst().setClipboard(std::string(text.begin() + selStart, text.begin() + selEnd));
					if(active && event.keysym.sym == SDLK_x) replaceSelection("");
				}
			}
			else if(active && event.keysym.sym == SDLK_v)
			{
				// einfügen
				const std::string& clipboard = GUI::inst().getClipboard();
				if(!clipboard.empty()) replaceSelection(clipboard);
			}
		}
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
	// alles auswählen
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

	// Signal auslösen
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

			// Signal auslösen
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

			// Signal auslösen
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
			// Bisher noch keine Auswahl!
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
		// Auswahl aufheben
		selStart = selEnd = 0;
	}

	this->cursor = cursor;
}

uint GUI_EditBox::getIndexAt(const Vec2i& position)
{
	std::vector<Vec2i> charPositions;
	p_font->measureText(text, 0, &charPositions);

	uint i;
	for(i = 0; i < static_cast<uint>(text.length()); i++)
	{
		if(charPositions[i].x + 6 >= position.x + scroll) break;
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