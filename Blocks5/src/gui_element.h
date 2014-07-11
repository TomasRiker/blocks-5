#ifndef _GUIELEMENT_H
#define _GUIELEMENT_H

/*** Klasse für ein GUI-Element ***/

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
	virtual void onRenderEnd();
	virtual void onUpdate();

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

	static uint numElementsRendered;

protected:
	bool useSkin() const;

	std::string name;
	GUI_Element* p_parent;
	std::list<GUI_Element*> children;
	Vec2i position;
	Vec2i size;
	bool visible;
	bool active;
	bool fill;
	Vec4d fillColor;
	std::string toolTip;
	bool toolTipOnly;
	int tabStop;
	Font* p_font;
};

#endif