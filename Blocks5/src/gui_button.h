#ifndef _GUI_BUTTON_H
#define _GUI_BUTTON_H

/*** Klasse für einen Button ***/

#include "gui_element.h"

class Texture;

class GUI_Button : public GUI_Element
{
public:
	DECL_CTOR(GUI_Button);
	~GUI_Button();

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
	INLINE_GETTER(Vec2i, getPositionOnTexture, positionOnTexture);
	INLINE_SETTER(Vec2i, getPositionOnTexture, positionOnTexture);
	INLINE_GETTER(Vec2i, getClickedPositionOnTexture, clickedPositionOnTexture);
	INLINE_SETTER(Vec2i, getClickedPositionOnTexture, clickedPositionOnTexture);

	INLINE_CONNECTOR(connectClicked, clicked);

private:
	std::string title;
	bool pushed;
	bool mouseOver;

	int style;
	std::string imageFilename;
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