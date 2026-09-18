#include "pch.h"
#include "gui_staticimage.h"
#include "texture.h"
#include "engine.h"

IMPL_CTOR(GUI_StaticImage)
{
	positionOnTexture = Vec2i(0, 0);
	color = Vec4f(1.0f, 1.0f, 1.0f, 1.0f);
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
		// render the image
		Engine::inst().renderSprite(p_image, Vec2i(0, 0), positionOnTexture, size, color);
	}
}

void GUI_StaticImage::readAttributes(TiXmlElement* p_element)
{
	TiXmlElement* e = p_element->FirstChildElement("Image");
	if(e)
	{
		const char* p_imageFilename = e->GetText();
		if(p_imageFilename) setRawImageFilename(p_imageFilename);

		e->QueryIntAttribute("u", &positionOnTexture.x);
		e->QueryIntAttribute("v", &positionOnTexture.y);
	}

	e = p_element->FirstChildElement("Color");
	if(e)
	{
		e->QueryFloatAttribute("r", &color.r);
		e->QueryFloatAttribute("g", &color.g);
		e->QueryFloatAttribute("b", &color.b);
		e->QueryFloatAttribute("a", &color.a);
	}
}

void GUI_StaticImage::setImageFilename(const std::string& imageFilename)
{
	if(p_image) p_image->release();
	this->imageFilename = imageFilename;
	p_image = Manager<Texture>::inst().request(imageFilename);
}

void GUI_StaticImage::setRawImageFilename(const std::string& rawImageFilename)
{
	this->rawImageFilename = rawImageFilename;
	setImageFilename(localizeString(rawImageFilename));
}

// The donation window's background carries $MM_DONATE_BACKGROUND_FILENAME.
// See GUI_Button::onUpdate - the same thing for the same reason.
void GUI_StaticImage::onUpdate()
{
	if(rawImageFilename.empty()) return;

	const std::string wanted = localizeString(rawImageFilename);
	if(wanted != imageFilename) setImageFilename(wanted);
}