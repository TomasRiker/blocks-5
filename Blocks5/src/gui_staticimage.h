#ifndef _GUI_STATICIMAGE_H
#define _GUI_STATICIMAGE_H

/*** Class for a static image ***/

#include "gui_element.h"

class Texture;

class GUI_StaticImage : public GUI_Element
{
public:
	DECL_CTOR(GUI_StaticImage);
	~GUI_StaticImage();

	void onRender();
	INLINE_GETTYPE("GUI_StaticImage");

	void readAttributes(TiXmlElement* p_element);

	INLINE_GETTER(std::string, getImageFilename, imageFilename);
	void setImageFilename(const std::string& imageFilename);
	// As in GUI_Button: the name from the XML can be a $ID and therefore
	// point to a different image depending on the language.
	void setRawImageFilename(const std::string& rawImageFilename);
	void onUpdate();
	INLINE_GETTER(Vec2i, getPositionOnTexture, positionOnTexture);
	INLINE_SETTER(Vec2i, getPositionOnTexture, positionOnTexture);
	INLINE_GETTER(Vec4d, getColor, color);
	INLINE_SETTER(Vec4d, setColor, color);

private:
	std::string imageFilename;
	std::string rawImageFilename;
	Vec2i positionOnTexture;
	Vec4d color;
	Texture* p_image;
};

#endif