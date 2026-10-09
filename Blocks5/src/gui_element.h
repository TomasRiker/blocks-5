#ifndef _GUIELEMENT_H
#define _GUIELEMENT_H

/*** Class for a GUI element ***/

#include "gui.h"
#include "font.h"

#define DECL_CTOR(NAME) NAME(const std::string& name, GUI_Element* p_parent, const Vec2i& position, const Vec2i& size);
#define IMPL_CTOR(NAME) NAME::NAME(const std::string& name, GUI_Element* p_parent, const Vec2i& position, const Vec2i& size) : GUI_Element(name, p_parent, position, size)
#define INLINE_GETTYPE(TYPESTRING) std::string getType() const { return TYPESTRING; }
#define INLINE_GETTER(TYPE, GETTERNAME, MEMBERNAME) const TYPE& GETTERNAME() const { return MEMBERNAME; }
#define INLINE_SETTER(TYPE, SETTERNAME, MEMBERNAME) void SETTERNAME(const TYPE& MEMBERNAME) { this->MEMBERNAME = MEMBERNAME; }
#define INLINE_PGETTER(TYPE, GETTERNAME, MEMBERNAME) TYPE GETTERNAME() { return MEMBERNAME; }
#define INLINE_PSETTER(TYPE, SETTERNAME, MEMBERNAME) void SETTERNAME(TYPE MEMBERNAME) { this->MEMBERNAME = MEMBERNAME; }
#define INLINE_CONNECTOR(CONNECTORNAME, MEMBERNAME) template<typename T> void CONNECTORNAME(T* p_destObject, void (T::*p_method)(GUI_Element*)) { MEMBERNAME.connect(p_destObject, p_method); }

class GUI_Element
{
public:
	DECL_CTOR(GUI_Element);
	virtual ~GUI_Element();

	void render();
	void update();
	virtual void onRender();
	virtual void onUpdate();

	// The rectangle the children are clipped to, in the element's own
	// coordinates; false for none. render() opens the renderer's scissor
	// scope around the children for it.
	virtual bool getClipRect(Vec2i* p_position, Vec2i* p_size) const;

	// An element that keeps these handlers can point at another one
	// (for="Name"), as <label for> does in a browser: a click on it toggles a
	// checkbox or radio button target and focuses any other, an edit box above
	// all. Not only text can be a label - the language flags in options.xml
	// are <StaticImage>. A button, a box or a list overrides them and so
	// ignores a for.
	virtual void onMouseDown(const Vec2i& position, int buttons);
	virtual void onMouseUp(const Vec2i& position, int buttons);
	virtual void onMouseEnter(int buttons);
	virtual void onMouseLeave(int buttons);
	virtual void onMouseMove(const Vec2i& position, const Vec2i& movement, int buttons);
	virtual void onMouseWheel(int dir);
	virtual void onKeyEvent(const SDL_KeyboardEvent& event);
	virtual void onTabbedIn();
	virtual std::string getType() const;

	GUI_Element* getElementAt(const Vec2i& position);
	// What counts as "hit": the element's own rectangle by default. A
	// checkbox or a radio button adds the caption it draws to its right, so a
	// click on the text counts like one on the box. Not const: the caption's
	// width is measured for it.
	virtual bool containsPoint(const Vec2i& position);
	// Whether a press at this point, in the element's own coordinates and
	// inside it, does something: what a finger that just missed may be moved
	// onto (GUI::pickTouchTarget). A control says yes, and so does a label
	// (for=); a frame, a text, a picture or a pane says no, and an element
	// that only carries a tooltip answers what its parent answers there,
	// since it hands every press on to it.
	virtual bool isClickTarget(const Vec2i& position);
	// Whether a finger dragged from this point, in the element's own
	// coordinates, moves what the element shows rather than pressing it - a
	// list's items. GUI then holds the finger's press back until it lifts,
	// a tap, which arrives as onMouseDown and onMouseUp in one tick, or moves
	// further than a tap may, after which the element hears onPan and never
	// the press. No by default.
	virtual bool pansAt(const Vec2i& position);
	// The finger panning the element moved by this much, or the glide after
	// it did. Whether the element went along, which it does not at an end:
	// that stops a glide.
	virtual bool onPan(const Vec2i& movement);
	// Whether this is a field a finger's tap means to type into: an active
	// edit box. The browser opens its text sheet for one (web_textsheet.cpp),
	// Windows its touch keyboard (TouchKeyboard). No by default.
	virtual bool takesText();
	// The element a press on this one ends up with: the control it labels,
	// the parent an element that only carries a tooltip hands it to, or
	// itself. Two elements with one receiver are one target to a finger.
	GUI_Element* getPressReceiver();
	void bringToFront();
	bool isFocused();
	bool isFocusedIndirectly();
	void focus();
	void center(bool h = true, bool v = true);

	bool load(const std::string& filename);
	bool load(TiXmlElement* p_element);
	virtual void readAttributes(TiXmlElement* p_element);

	GUI_Element* getChild(const std::string& name);
	GUI_Element* operator [] (const std::string& name);
	bool isChildOf(GUI_Element* p_element) const;

	GUI_Element* getNextTabElement();
	GUI_Element* getPreviousTabElement();

	const std::string& getName() const;
	std::string getFullName() const;
	GUI_Element* getParent();
	const std::list<GUI_Element*>& getChildren() const;
	const Vec2i& getPosition() const;
	Vec2i getAbsPosition() const;
	void setPosition(const Vec2i& position);
	void setAbsPosition(const Vec2i& absPosition);
	const Vec2i& getSize() const;
	void setSize(const Vec2i& size);
	bool isVisible() const;
	bool isReallyVisible() const;
	void show();
	void hide();
	bool isActive() const;
	void activate();
	void deactivate();
	const std::string& getToolTip() const;
	void setToolTip(const std::string& toolTip);
	bool isToolTipOnly() const;
	void setToolTipOnly(bool toolTipOnly);
	int getTabStop() const;
	void setTabStop(int tabStop);
	INLINE_PGETTER(Font*, getFont, p_font);
	INLINE_GETTER(std::string, getLinkedElement, linkedElement);
	INLINE_SETTER(std::string, setLinkedElement, linkedElement);

	static uint numElementsRendered;

protected:
	bool useSkin() const;
	void renderChildren();

	// Where a checkbox or radio button draws a caption of this size: the same
	// gap after the box in every font, and centred on it.
	Vec2i getCaptionPosition(const Vec2i& captionSize) const;

	// The linked element, looked up relative to this element's own parent.
	// 0 when nothing is linked or the name points nowhere.
	GUI_Element* getLinkedTarget();

	std::string name;
	std::string linkedElement;
	GUI_Element* p_parent;
	std::list<GUI_Element*> children;
	Vec2i position;
	Vec2i size;
	bool visible;
	bool active;
	bool fill;
	Vec4f fillColor;
	std::string toolTip;
	bool toolTipOnly;
	int tabStop;
	Font* p_font;
};

#endif