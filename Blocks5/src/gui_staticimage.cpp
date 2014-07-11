#include "pch.h"
#include "gui_staticimage.h"
#include "texture.h"
#include "engine.h"

IMPL_CTOR(GUI_StaticImage)
{
	positionOnTexture = Vec2i(0, 0);
	color = Vec4d(1.0, 1.0, 1.0, 1.0);
	p_image = 0;
}

GUI_StaticImage::~GUI_StaticImage()
{
	if(p_image) p_image->release();
}

void GUI_StaticImage::onRender()
{
	if(p_image)
	{
		// Bild rendern
		Engine::inst().renderSprite(p_image, Vec2i(0, 0), positionOnTexture, size, color);
	}
}

void GUI_StaticImage::readAttributes(TiXmlElement* p_element)
{
	TiXmlElement* e = p_element->FirstChildElement("Image");
	if(e)
	{
		const char* p_imageFilename = e->GetText();
		if(p_imageFilename) setImageFilename(localizeString(p_imageFilename));

		e->QueryIntAttribute("u", &positionOnTexture.x);
		e->QueryIntAttribute("v", &positionOnTexture.y);
	}

	e = p_element->FirstChildElement("Color");
	if(e)
	{
		e->QueryDoubleAttribute("r", &color.r);
		e->QueryDoubleAttribute("g", &color.g);
		e->QueryDoubleAttribute("b", &color.b);
		e->QueryDoubleAttribute("a", &color.a);
	}
}

void GUI_StaticImage::setImageFilename(const std::string& imageFilename)
{
	if(p_image) p_image->release();
	this->imageFilename = imageFilename;
	p_image = Manager<Texture>::inst().request(imageFilename);
}