#ifndef _GUI_SCROLLBAR_H
#define _GUI_SCROLLBAR_H

/*** Klasse für eine Scroll-Bar ***/

#include "gui_element.h"

class GUI_StaticText;

class GUI_ScrollBar : public GUI_Element
{
public:
	DECL_CTOR(GUI_ScrollBar);
	~GUI_ScrollBar();

	void onRender();
	void onUpdate();
	void onMouseDown(const Vec2i& position, int buttons);
	void onMouseUp(const Vec2i& position, int buttons);
	void onMouseMove(const Vec2i& position, const Vec2i& movement, int buttons);
	INLINE_GETTYPE("GUI_ScrollBar");

	void readAttributes(TiXmlElement* p_element);

	INLINE_GETTER(int, getScroll, scroll);
	void setScroll(int scroll);
	INLINE_GETTER(int, getAreaSize, areaSize);
	void setAreaSize(int areaSize);
	INLINE_GETTER(int, getPageSize, pageSize);
	void setPageSize(int pageSize);
	INLINE_PGETTER(GUI_StaticText*, getReceiver, p_receiver);
	void setReceiver(GUI_StaticText* p_receiver);

	INLINE_CONNECTOR(connectChanged, changed);

private:
	void updateValues();
	void setDragBarY(int dragBarY);
	void updateReceiver();

	bool dir;
	int scroll;
	int areaSize;
	int pageSize;
	int dragBarY;
	int dragBarHeight;
	int dragStartY;
	bool dragging;
	bool pushedUp;
	bool pushedDown;
	int pushTime;
	GUI_StaticText* p_receiver;

	sigslot::signal1<GUI_Element*> changed;
};

#endif