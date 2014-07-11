#ifndef _GUI_STATICIMAGE_H
#define _GUI_STATICIMAGE_H

/*** Klasse für ein statisches Bild ***/

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
	INLINE_GETTER(Vec2i, getPositionOnTexture, positionOnTexture);
	INLINE_SETTER(Vec2i, getPositionOnTexture, positionOnTexture);
	INLINE_GETTER(Vec4d, getColor, color);
	INLINE_SETTER(Vec4d, setColor, color);

private:
	std::string imageFilename;
	Vec2i positionOnTexture;
	Vec4d color;
	Texture* p_image;
};

#endif