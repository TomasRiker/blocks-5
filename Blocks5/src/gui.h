#ifndef _GUI_H
#define _GUI_H

/*** Class for the user interface ***/

class GUI_Element;
class Font;
class Texture;

class GUI : public Singleton<GUI>
{
	friend class Singleton<GUI>;
	friend class GUI_Element;

public:
	bool init();
	void exit();
	void render();
	void renderToolTip();
	void display();
	void update();

	// A skin frame, its texels times color: a flashing button draws a tinted
	// copy over its own.
	void renderFrame(const Vec2i& targetPosition, const Vec2i& size, const Vec2i& positionOnTexture,
					 const Vec4f& color = Vec4f(1.0f, 1.0f, 1.0f, 1.0f));

	GUI_Element* getElement(const std::string& fullName);
	GUI_Element* operator [] (const std::string& fullName);

	GUI_Element* getRoot();
	Font* getFont();
	Font* getToolTipFont();
	float getOpacity() const;
	void setOpacity(float opacity);
	Texture* getSkin();
	void setSkin(Texture* p_skin);
	const Vec2i& getCursorPos() const;
	GUI_Element* getFocusElement();

	// Is the key just arriving in onKeyEvent() the repeat of a held one?
	// Anything that reads it as a command - Escape, Return, the editors'
	// shortcuts - must skip such a repeat, or a held key triggers the command
	// again every 60 ms. Edit boxes and the list want repeats for typing and
	// moving, and ask only before Return clicks a submit button.
	bool isKeyRepeat() const;
	void setFocusElement(GUI_Element* p_element);
	GUI_Element* getOldFocusElement();
	GUI_Element* getMouseDownElement();
	const std::string& getClipboard() const;
	void setClipboard(const std::string& clipboard);

	// What a finger pressing at this point means to press, and where in it:
	// the element under the point where that takes a press, enabled or not;
	// otherwise the nearest one that does within a finger's reach, at its
	// nearest point; otherwise - nothing in reach, or another one about as
	// near - the element under the point after all, which is what a mouse
	// would get. update() asks it for a finger's press, the test hooks for
	// any point.
	GUI_Element* pickTouchTarget(const Vec2i& point, Vec2i* p_landing);

private:
	GUI();
	~GUI();

	bool initialized;
	GUI_Element* p_root;
	Font* p_font;
	Font* p_toolTipFont;
	uint texID;
	float opacity;
	Texture* p_skin;

	// Valid only during an onKeyEvent(); isKeyRepeat() reads it.
	bool keyRepeat;

	Vec2i cursorPos;
	Vec2i oldCursorPos;
	// The cursor position as the window reports it - see update().
	Vec2i oldRawCursorPos;
	GUI_Element* p_elementAtCursor;
	GUI_Element* p_oldElementAtCursor;
	GUI_Element* p_focusElement;
	GUI_Element* p_oldFocusElement;
	GUI_Element* p_mouseDownElement;
	std::string clipboard;
	uint noMoveCounter;

	// The element a finger's press went to and how far the press was moved
	// to reach it, for the whole gesture: that element is told every position
	// until the finger lifts moved by the same offset (pointFor). 0 for a
	// mouse's press, and between presses.
	GUI_Element* p_fingerElement;
	Vec2i fingerOffset;
	// Whether that element is still the one under the finger, which it stays
	// while the finger is within reach of it and it is on screen.
	bool fingerHolds;

	// Whether a finger at this point still reaches the element: somewhere
	// within its reach the element is what a press would hit.
	bool fingerReaches(GUI_Element* p_element, const Vec2i& point);
	// Where an element is told the cursor is: the finger's position moved by
	// the press's offset for the element that press went to, the cursor's
	// own for any other.
	Vec2i pointFor(GUI_Element* p_element) const;
};

#endif