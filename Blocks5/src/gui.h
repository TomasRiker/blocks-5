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
	// nearest pixel; otherwise - nothing in reach, another one about as near,
	// or the nearest greyed out - the element under the point after all,
	// which is what a mouse would get. update() asks it for a finger's press,
	// the test hooks for any point.
	GUI_Element* pickTouchTarget(const Vec2i& point, Vec2i* p_landing);
	// The text field a finger tapped, pressing at one point and going no
	// further from it than the other: what a press there picks, or the field
	// a label it picks stands for, where that takes text (takesText). 0 for a
	// drag or anything else. The browser asks it as the finger lifts, before
	// the game has handled the touch, so it goes by the two points alone and
	// not by the state of the gesture (web_textsheet.cpp).
	GUI_Element* textFieldTapped(const Vec2i& press, const Vec2i& farthest);
	// Whether a finger's tap has asked for the touch keyboard and the focus
	// has stayed in a text field since (TouchKeyboard).
	bool isTouchKeyboardWanted() const;

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

	// A finger's press on an element that pans (GUI_Element::pansAt), held
	// back from it while the finger may still be tapping: the element, 0 when
	// no finger is on one. Past the slop it pans (`panning`), and then hears
	// onPan for every move. Caught, the press stopped a glide of that element
	// and is no tap when it lifts.
	GUI_Element* p_panElement;
	bool panning;
	bool panCaught;
	// The press in the element's coordinates, for a tap's onMouseDown; where
	// it was on the screen (pointFor) and how far the finger may go from
	// there and still tap, in game pixels; the point the element followed
	// last, and when by the clock (SDL_GetTicks) it last moved; and the
	// finger's point at the end of each of the last ticks, newest first, for
	// its speed when it lifts.
	Vec2i panPress;
	Vec2i panStart;
	float panSlop;
	Vec2i panPoint;
	Uint32 panMovedAt;
	static const int PAN_TRAIL = 6;
	Vec2i panTrail[PAN_TRAIL];
	int panTrailLength;

	// An element let go of while it was panning glides on: the element, 0 for
	// none, its speed in game pixels a tick, the fraction of a pixel it has
	// yet to move, the share of the speed a tick keeps, and the speed it
	// stops below.
	GUI_Element* p_glideElement;
	bool touchKeyboardWanted;
	Vec2f glideSpeed;
	Vec2f glideRest;
	float glideKeep;
	float glideStop;

	// Whether a finger at this point still reaches the element: somewhere
	// within its reach the element is what a press would hit.
	bool fingerReaches(GUI_Element* p_element, const Vec2i& point);
	// Where an element is told the cursor is: the finger's position moved by
	// the press's offset for the element that press went to, the cursor's
	// own for any other.
	Vec2i pointFor(GUI_Element* p_element) const;
	// The panning finger at this point: past the slop the element follows it.
	void followPan(const Vec2i& point);
	// The panning finger lifted, last at this point: a tap, or the speed for
	// a glide.
	void releasePan(const Vec2i& point);
	// One tick of the glide.
	void glide();
	// A finger's press or tap has been handed over: where it put the focus
	// in a text field, the touch keyboard is asked for.
	void fingerFocused();
};

#endif