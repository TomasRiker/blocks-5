#ifndef _GUI_BUTTON_H
#define _GUI_BUTTON_H

/*** Class for a button ***/

#include "gui_element.h"

class Texture;

class GUI_Button : public GUI_Element
{
public:
	DECL_CTOR(GUI_Button);
	~GUI_Button();

	bool containsPoint(const Vec2i& position);
	void onRender();
	void onUpdate();
	void onMouseDown(const Vec2i& position, int buttons);
	void onMouseUp(const Vec2i& position, int buttons);
	void onMouseEnter(int buttons);
	void onMouseLeave(int buttons);
	INLINE_GETTYPE("GUI_Button");

	void click();

	void readAttributes(TiXmlElement* p_element);

	INLINE_GETTER(std::string, getTitle, title);
	INLINE_SETTER(std::string, setTitle, title);

	INLINE_GETTER(std::string, getImageFilename, imageFilename);
	void setImageFilename(const std::string& imageFilename);
	// The name as it stands in the XML. If it is a $ID it can point to a
	// different image depending on the language; onUpdate therefore resolves
	// it afresh.
	void setRawImageFilename(const std::string& rawImageFilename);
	INLINE_GETTER(Vec2i, getPositionOnTexture, positionOnTexture);
	INLINE_SETTER(Vec2i, setPositionOnTexture, positionOnTexture);
	INLINE_GETTER(Vec2i, getClickedPositionOnTexture, clickedPositionOnTexture);
	INLINE_SETTER(Vec2i, setClickedPositionOnTexture, clickedPositionOnTexture);

	INLINE_CONNECTOR(connectClicked, clicked);

private:
	std::string title;
	bool pushed;
	bool mouseOver;

	int style;

	// How many pixels of the cell all round are only border. A cell in
	// buttons.png is larger than the disc inside it - the rest belongs to
	// the drop shadow and is transparent. Without this inset a button would
	// be clickable where nothing is to be seen, and in the level selection
	// neighbouring buttons would reach into each other's disc.
	int imageInset;

	std::string imageFilename;
	std::string rawImageFilename;
	Vec2i positionOnTexture;
	Vec2i clickedPositionOnTexture;
	Vec4d stdColor;
	Vec4d hoverColor;
	Vec4d currentColor;
	double stdScaling;
	double hoverScaling;
	double currentScaling;
	Texture* p_image;

	sigslot::signal1<GUI_Element*> clicked;
};

#endif