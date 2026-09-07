#ifndef _GUI_RADIOBUTTON_H
#define _GUI_RADIOBUTTON_H

/*** Class for a radio button ***/

#include "gui_element.h"

class Texture;

class GUI_RadioButton : public GUI_Element
{
public:
	DECL_CTOR(GUI_RadioButton);
	~GUI_RadioButton();

	void onRender();
	void onMouseDown(const Vec2i& position, int buttons);
	void onMouseUp(const Vec2i& position, int buttons);
	void onMouseEnter(int buttons);
	// The caption beside the box counts as part of the hit area.
	bool containsPoint(const Vec2i& position);
	void onMouseLeave(int buttons);
	INLINE_GETTYPE("GUI_RadioButton");

	void readAttributes(TiXmlElement* p_element);

	INLINE_GETTER(std::string, getTitle, title);
	INLINE_SETTER(std::string, setTitle, title);
	INLINE_GETTER(uint, getGroup, group);
	void setGroup(uint group);
	INLINE_GETTER(bool, getButtonLook, buttonLook);
	INLINE_SETTER(bool, setButtonLook, buttonLook);
	INLINE_GETTER(bool, isChecked, checked);
	// check() is the user's click: it fires the changed signal.
	// setChecked() only makes the display catch up.
	void check();
	void setChecked();

	INLINE_GETTER(std::string, getImageFilename, imageFilename);
	void setImageFilename(const std::string& imageFilename);
	INLINE_GETTER(Vec2i, getPositionOnTexture, positionOnTexture);
	INLINE_SETTER(Vec2i, getPositionOnTexture, positionOnTexture);

	INLINE_CONNECTOR(connectChanged, changed);

private:
	std::string title;
	uint group;
	bool buttonLook;
	bool checked;
	bool pushed;
	bool mouseOver;

	std::string imageFilename;
	Vec2i positionOnTexture;
	Texture* p_image;

	sigslot::signal1<GUI_Element*> changed;
};

#endif