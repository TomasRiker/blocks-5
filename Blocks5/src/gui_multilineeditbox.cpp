#include "pch.h"
#include "gui_multilineeditbox.h"
#include "gui_scrollbar.h"
#include "engine.h"

IMPL_CTOR(GUI_MultiLineEditBox)
{
	cursor = selStart = selEnd = 0;
	scroll = Vec2i(0, 0);
	text = "MultiLineEditBox";

	// Scroll-Bars erzeugen
	p_scrollBarH = new GUI_ScrollBar("ScrollBarH", this, Vec2i(0, size.y - 16), Vec2i(size.x - 16, 16));
	p_scrollBarV = new GUI_ScrollBar("ScrollBarV", this, Vec2i(size.x - 16, 0), Vec2i(16, size.y - 16));
	p_scrollBarH->connectChanged(this, &GUI_MultiLineEditBox::handleScrollBarChanged);
	p_scrollBarV->connectChanged(this, &GUI_MultiLineEditBox::handleScrollBarChanged);

	measureText();
	makeCursorVisible();
	updateScrollBars();
}

GUI_MultiLineEditBox::~GUI_MultiLineEditBox()
{
}

void GUI_MultiLineEditBox::onRender()
{
	GUI& gui = GUI::inst();
	bool focused = isFocusedIndirectly();

	if(useSkin())
	{
		// Eingabefeld zeichnen
		gui.renderFrame(Vec2i(0, 0), size - Vec2i(16, 16), focused ? Vec2i(48, 144) : Vec2i(0, 144));
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
	glScissor(pos.x + 2, h - pos.y - size.y + 18, size.x - 20, size.y - 20);

	// Position des Cursors berechnen
	Vec2i c = textCharPositions[cursor];

	glPushMatrix();
	glTranslated(-scroll.x, -scroll.y, 0.0);

	// Text schreiben
	p_font->renderText(text, Vec2i(4, 2), active ? Vec4d(1.0, 1.0, 1.0, 1.0) : Vec4d(0.5, 0.5, 0.5, 1.0));

	if(focused)
	{
		if(selStart != selEnd)
		{
			// Auswahl zeichnen
			glBegin(GL_QUADS);

			int h = p_font->getLineHeight();
			for(uint i = selStart; i < selEnd; i++)
			{
				const Vec2i p = textCharPositions[i];
				int w = max(0, textCharPositions[i + 1].x - p.x);
				glColor4d(0.25, 0.25, 1.0, 0.5);
				glVertex2i(p.x, p.y + 1);
				glVertex2i(p.x + w, p.y + 1);
				glColor4d(0.2, 0.2, 0.8, 0.5);
				glVertex2i(p.x + w, p.y + 1 + h);
				glVertex2i(p.x, p.y + 1 + h);
			}

			glEnd();
		}

		// Cursor zeichnen
		glBegin(GL_LINES);
		double alpha = 0.6 + 0.4 * sin(0.02 * Engine::inst().getTime());
		if(active) glColor4d(1.0, 1.0, 1.0, alpha);
		else glColor4d(0.5, 0.5, 0.5, alpha);
		glVertex2i(c.x, c.y + 1);
		glVertex2i(c.x, c.y + 1 + p_font->getLineHeight());
		glEnd();
	}

	glPopMatrix();
	glDisable(GL_SCISSOR_TEST);
}

void GUI_MultiLineEditBox::setText(const std::string& text)
{
	this->text = text;
	cursor = 0;
	scroll = Vec2i(0, 0);
	selStart = selEnd = 0;
	measureText();
	makeCursorVisible();
	updateScrollBars();

	// Signal auslösen
	changed(this);
}

void GUI_MultiLineEditBox::onMouseDown(const Vec2i& position,
									   int buttons)
{
	if(buttons & 1)
	{
		bool shift = Engine::inst().isKeyDown(SDLK_LSHIFT) || Engine::inst().isKeyDown(SDLK_RSHIFT);
		setCursor(getIndexAt(position), shift);
	}
}

void GUI_MultiLineEditBox::onMouseMove(const Vec2i& position,
									   const Vec2i& movement,
									   int buttons)
{
	if(GUI::inst().getMouseDownElement() == this && (buttons & 1)) setCursor(getIndexAt(position), true);
}

void GUI_MultiLineEditBox::onMouseWheel(int dir)
{
	p_scrollBarV->setScroll(p_scrollBarV->getScroll() + dir * 4 * p_font->getLineHeight());
}

void GUI_MultiLineEditBox::onKeyEvent(const SDL_KeyboardEvent& event)
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
	case SDLK_UP:
	case SDLK_DOWN:
	case SDLK_PAGEUP:
	case SDLK_PAGEDOWN:
		{
			Vec2i c = textCharPositions[cursor] + Vec2i(2, 0);
			if(event.keysym.sym == SDLK_UP) setCursor(getIndexAt(c - scroll + Vec2i(0, -4)), shift);
			else if(event.keysym.sym == SDLK_DOWN) setCursor(getIndexAt(c - scroll + Vec2i(0, p_font->getLineHeight() + 4)), shift);
			else if(event.keysym.sym == SDLK_PAGEUP) setCursor(getIndexAt(c - scroll + Vec2i(0, -size.y - 4)), shift);
			else if(event.keysym.sym == SDLK_PAGEDOWN) setCursor(getIndexAt(c - scroll + Vec2i(0, p_font->getLineHeight() + size.y + 4)), shift);
		}
		break;
	case SDLK_HOME:
		setCursor(ctrl ? 0 : findLineBegin(cursor), shift);
		break;
	case SDLK_END:
		setCursor(ctrl ? static_cast<uint>(text.length()) : findLineEnd(cursor), shift);
		break;
	case SDLK_DELETE:
		if(active) del();
		break;
	case SDLK_BACKSPACE:
		if(active) backspace();
		break;
	case SDLK_RETURN:
		if(active) replaceSelection("\n");
		break;
	case SDLK_a:
	case SDLK_c:
	case SDLK_v:
	case SDLK_x:
		if(ctrl)
		{
			if(event.keysym.sym == SDLK_a)
			{
				// Alles auswählen
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

void GUI_MultiLineEditBox::replaceSelection(const std::string& replacement)
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

	measureText();
	makeCursorVisible();
	updateScrollBars();

	// Signal auslösen
	changed(this);
}

void GUI_MultiLineEditBox::del()
{
	if(selStart == selEnd)
	{
		if(cursor < static_cast<uint>(text.length()))
		{
			const std::string textBeforeCursor(text.begin(), text.begin() + cursor);
			const std::string textAfterCursor(text.begin() + cursor + 1, text.end());
			text = textBeforeCursor + textAfterCursor;
			cursor = static_cast<uint>(textBeforeCursor.length());

			measureText();
			makeCursorVisible();
			updateScrollBars();

			// Signal auslösen
			changed(this);
		}
	}
	else
	{
		replaceSelection("");
	}
}

void GUI_MultiLineEditBox::backspace()
{
	if(selStart == selEnd)
	{
		if(cursor)
		{
			const std::string textBeforeCursor(text.begin(), text.begin() + cursor - 1);
			const std::string textAfterCursor(text.begin() + cursor, text.end());
			text = textBeforeCursor + textAfterCursor;
			cursor = static_cast<uint>(textBeforeCursor.length());

			measureText();
			makeCursorVisible();
			updateScrollBars();

			// Signal auslösen
			changed(this);
		}
	}
	else 
	{
		replaceSelection("");
	}
}

void GUI_MultiLineEditBox::setCursor(uint cursor,
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

	makeCursorVisible();
}

uint GUI_MultiLineEditBox::getIndexAt(const Vec2i& position)
{
	uint i;
	for(i = 0; i < static_cast<uint>(text.length()); i++)
	{
		if(textCharPositions[i].y + p_font->getLineHeight() >= position.y + scroll.y)
		{
			if(textCharPositions[i].x + 6 >= position.x + scroll.x) break;
			else if(text[i] == '\n') break;
			else if(i == static_cast<uint>(text.length() - 1))
			{
				i++;
				break;
			}
		}
	}

	return i;
}

uint GUI_MultiLineEditBox::findLineBegin(uint cursor) const
{
	while(true)
	{
		if(!cursor) return 0;
		cursor--;
		if(text[cursor] == '\n') return cursor + 1;
	}
}

uint GUI_MultiLineEditBox::findLineEnd(uint cursor) const
{
	if(cursor >= static_cast<uint>(text.length())) return static_cast<uint>(text.length());
	if(text[cursor] == '\n') return cursor;

	while(true)
	{
		if(cursor == static_cast<uint>(text.length() - 1)) return cursor + 1;
		cursor++;
		if(text[cursor] == '\n') return cursor;
	}
}

void GUI_MultiLineEditBox::makeCursorVisible()
{
	// Position des Cursors berechnen
	Vec2i c = textCharPositions[cursor];

	// Cursor unsichtbar?
	Vec2i vc = c - scroll;
	if(vc.x < 16) scroll.x -= 16 - vc.x;
	else if(vc.x > size.x - 32) scroll.x += vc.x - (size.x - 32);
	if(vc.y < 16) scroll.y -= 16 - vc.y;
	else if(vc.y > size.y - 32) scroll.y += vc.y - (size.y - 32);
	if(scroll.x < 0) scroll.x = 0;
	if(scroll.y < 0) scroll.y = 0;

	updateScrollBars();
}

void GUI_MultiLineEditBox::measureText()
{
	textCharPositions.clear();
	p_font->measureText(text, &textDim, &textCharPositions, Vec2i(4, 2));
}

void GUI_MultiLineEditBox::updateScrollBars()
{
	p_scrollBarH->setAreaSize(textDim.x + 32);
	p_scrollBarH->setPageSize(size.x - 20);
	p_scrollBarH->setScroll(scroll.x);
	p_scrollBarV->setAreaSize(textDim.y + 32);
	p_scrollBarV->setPageSize(size.y - 20);
	p_scrollBarV->setScroll(scroll.y);
}

void GUI_MultiLineEditBox::handleScrollBarChanged(GUI_Element* p_element)
{
	// Scrolling setzen
	scroll = Vec2i(p_scrollBarH->getScroll(), p_scrollBarV->getScroll());
}

void GUI_MultiLineEditBox::readAttributes(TiXmlElement* p_element)
{
	TiXmlElement* e = p_element->FirstChildElement("Text");
	if(e)
	{
		const char* p_text = e->GetText();
		setText(p_text ? p_text : "");
	}
}