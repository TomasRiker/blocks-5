#include "pch.h"

// web_textsheet.cpp - typing on a touchscreen in the browser. A phone shows
// its keyboard only for a focused field of the page, never for the canvas, and
// what its keyboard types arrives as edits of that field rather than as keys:
// Android's keyboards report key 229 and no character, and autocorrect, swipe
// typing and dictation replace whole words with no key at all. So a finger's
// tap on one of the game's text fields opens a sheet of the page's own - a
// caption, a real field holding the text, OK and Cancel (shell.html, pre.js) -
// and OK writes what the field holds back into the game's. A mouse's click
// and a keyboard's typing go on as before.

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include "engine.h"
#include "gui.h"
#include "gui_element.h"
#include "gui_editbox.h"
#include "gui_multilineeditbox.h"
#include "gui_statictext.h"
#include "gui_window.h"

namespace
{
	// The field the open sheet edits, by its full name: the sheet stays open
	// across ticks, and by the time it closes the field can be gone.
	std::string sheetField;

	// What the sheet is headed with: the label that stands for the field
	// (for=), else the title of the window the field is in, else nothing.
	std::string captionFor(GUI_Element* p_field)
	{
		std::string caption;
		GUI_Element* p_parent = p_field->getParent();
		if(p_parent)
		{
			const std::list<GUI_Element*>& siblings = p_parent->getChildren();
			for(std::list<GUI_Element*>::const_iterator i = siblings.begin(); i != siblings.end() && caption.empty(); ++i)
			{
				GUI_StaticText* p_label = dynamic_cast<GUI_StaticText*>(*i);
				if(p_label && p_label != p_field && p_label->getPressReceiver() == p_field) caption = p_label->getDrawnText();
			}
		}
		for(GUI_Element* p_element = p_parent; p_element && caption.empty(); p_element = p_element->getParent())
		{
			GUI_Window* p_window = dynamic_cast<GUI_Window*>(p_element);
			if(p_window) caption = Engine::inst().localizeString(p_window->getTitle());
		}

		// One line: a label wraps to its own width, the sheet to its own.
		for(std::string::size_type i = 0; i < caption.length(); i++)
		{
			if(caption[i] == '\n') caption[i] = ' ';
		}
		return caption;
	}
}

extern "C"
{
	// A finger lifted: it pressed at x0, y0 and went no further from there
	// than x1, y1, both in the canvas's pixels, which are the window's. Where
	// that was a tap on a text field (GUI::textFieldTapped), the sheet opens
	// for it - here, inside the touch's own handler, the only place a page
	// may focus a field and have iOS show its keyboard.
	EMSCRIPTEN_KEEPALIVE void blocks5_textFieldTapped(int x0, int y0, int x1, int y1)
	{
		GUI& gui = GUI::inst();
		if(!gui.getRoot() || !sheetField.empty()) return;

		// A finger that pressed or went beside the picture, in a black bar,
		// pressed nothing there or let go (GUI::update).
		Engine& engine = Engine::inst();
		if(!engine.isOnPicture(Vec2i(x0, y0)) || !engine.isOnPicture(Vec2i(x1, y1))) return;
		GUI_Element* p_field = gui.textFieldTapped(engine.windowToGame(Vec2i(x0, y0)), engine.windowToGame(Vec2i(x1, y1)));
		if(!p_field) return;

		std::string text;
		int multiline = 0, verbatim = 0;
		GUI_EditBox* p_line = dynamic_cast<GUI_EditBox*>(p_field);
		GUI_MultiLineEditBox* p_lines = dynamic_cast<GUI_MultiLineEditBox*>(p_field);
		if(p_line)
		{
			text = p_line->getText();
			verbatim = p_line->isVerbatim() ? 1 : 0;
		}
		else if(p_lines)
		{
			text = p_lines->getText();
			multiline = 1;
		}
		else return;

		const std::string caption = captionFor(p_field);
		const std::string ok = engine.localizeString("$OK");
		const std::string cancel = engine.localizeString("$CANCEL");

		// Every string is Latin-1, a byte a character, as the page's field
		// holds it: no UTF-8 on the way.
		const int opened = EM_ASM_INT({
			function latin1(p, n) { var s = ''; for (var i = 0; i < n; i++) s += String.fromCharCode(HEAPU8[p + i]); return s; }
			return Module['b5_openTextSheet'](latin1($0, $1), latin1($2, $3), $4 != 0, $5 != 0, latin1($6, $7), latin1($8, $9)) ? 1 : 0;
		}, text.data(), static_cast<int>(text.size()), caption.data(), static_cast<int>(caption.size()),
		   multiline, verbatim, ok.data(), static_cast<int>(ok.size()), cancel.data(), static_cast<int>(cancel.size()));
		if(opened) sheetField = p_field->getFullName();
	}

	// The sheet closed. With OK, Module.b5_sheetText holds what the field
	// said, already cut down by pre.js to what the game's field can hold, and
	// it replaces the field's text - only where it differs, since setting it
	// tells the editors something changed.
	EMSCRIPTEN_KEEPALIVE void blocks5_textSheetClosed(int ok)
	{
		GUI_Element* p_field = sheetField.empty() ? 0 : GUI::inst().getElement(sheetField);
		sheetField.clear();
		if(!ok || !p_field) return;

		const int length = EM_ASM_INT({ return Module['b5_sheetText'].length; });
		std::string text(static_cast<std::string::size_type>(length > 0 ? length : 0), ' ');
		if(length > 0)
		{
			EM_ASM({
				var s = Module['b5_sheetText'];
				for (var i = 0; i < s.length; i++) HEAPU8[$0 + i] = s.charCodeAt(i);
			}, &text[0]);
		}

		// The caret goes to the end, wherever it stood in the sheet: setText
		// puts it at the start.
		GUI_EditBox* p_line = dynamic_cast<GUI_EditBox*>(p_field);
		GUI_MultiLineEditBox* p_lines = dynamic_cast<GUI_MultiLineEditBox*>(p_field);
		const uint end = static_cast<uint>(text.length());
		if(p_line && p_line->getText() != text)
		{
			p_line->setText(text);
			p_line->setCursor(end, false);
		}
		else if(p_lines && p_lines->getText() != text)
		{
			p_lines->setText(text);
			p_lines->setCursor(end, false);
		}
	}
}

#endif
