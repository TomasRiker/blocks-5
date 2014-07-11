#ifndef _GUI_MULTILINEEDITBOX_H
#define _GUI_MULTILINEEDITBOX_H

/*** Klasse für ein mehrzeiliges Eingabefeld ***/

#include "gui_element.h"

class GUI_Button;
class GUI_ScrollBar;

class GUI_MultiLineEditBox : public GUI_Element, public sigslot::has_slots<>
{
public:
	DECL_CTOR(GUI_MultiLineEditBox);
	~GUI_MultiLineEditBox();

	void onRender();
	void onMouseDown(const Vec2i& position, int buttons);
	void onMouseMove(const Vec2i& position, const Vec2i& movement, int buttons);
	void onMouseWheel(int dir);
	void onKeyEvent(const SDL_KeyboardEvent& event);
	INLINE_GETTYPE("GUI_MultiLineEditBox");

	void readAttributes(TiXmlElement* p_element);

	INLINE_GETTER(std::string, getText, text);
	void setText(const std::string& text);
	INLINE_GETTER(uint, getCursor, cursor);

	INLINE_CONNECTOR(connectChanged, changed);

private:
	void replaceSelection(const std::string& replacement);
	void del();
	void backspace();
	void setCursor(uint cursor, bool shift);
	uint getIndexAt(const Vec2i& position);
	uint findLineBegin(uint cursor) const;
	uint findLineEnd(uint cursor) const;
	void makeCursorVisible();
	void measureText();
	void updateScrollBars();

	void handleScrollBarChanged(GUI_Element* p_element);

	std::string text;
	uint cursor;
	uint selStart;
	uint selEnd;
	Vec2i scroll;
	GUI_ScrollBar* p_scrollBarH;
	GUI_ScrollBar* p_scrollBarV;
	Vec2i textDim;
	std::vector<Vec2i> textCharPositions;

	sigslot::signal1<GUI_Element*> changed;
};

#endif