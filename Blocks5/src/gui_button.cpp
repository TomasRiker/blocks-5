#include "pch.h"
#include "gui_button.h"
#include "engine.h"
#include "texture.h"

IMPL_CTOR(GUI_Button)
{
	title = "Button";
	pushed = false;
	mouseOver = false;

	style = 0;
	imageInset = 0;
	positionOnTexture = Vec2i(0, 0);
	clickedPositionOnTexture = Vec2i(0, 0);
	p_image = 0;

	stdColor = Vec4f(1.0f, 1.0f, 1.0f, 0.85f);
	hoverColor = Vec4f(1.0f, 1.0f, 1.0f, 1.0f);
	currentColor = stdColor;

	stdScaling = 1.0f;
	hoverScaling = 1.0f;
	currentScaling = stdScaling;
}

GUI_Button::~GUI_Button()
{
	if(p_image) p_image->release();
}

bool GUI_Button::containsPoint(const Vec2i& position)
{
	return position.x >= imageInset &&
		   position.y >= imageInset &&
		   position.x < size.x - imageInset &&
		   position.y < size.y - imageInset;
}

void GUI_Button::onRender()
{
	GUI& gui = GUI::inst();
	int offset = 0;

	if(useSkin())
	{
		if(style == 0)
		{
			// draw the button
			gui.renderFrame(Vec2i(0, 0), size, pushed && mouseOver ? Vec2i(48, 96) : Vec2i(0, 96));
			offset = -1;
		}
		else
		{
			// With no image the button stays invisible and is clickable all
			// the same - that is how menu.xml puts a button over the address
			// belonging to the background image. renderSprite would
			// otherwise have dereferenced the null pointer.
			if(p_image)
			{
				Vec2i t = positionOnTexture;
				if(pushed && mouseOver) t = clickedPositionOnTexture;

				Renderer& renderer = Renderer::inst();
				renderer.push();
				renderer.translate(static_cast<float>(size.x / 2), static_cast<float>(size.y / 2));
				renderer.scale(currentScaling, currentScaling);
				Engine::inst().renderSprite(p_image, -size / 2, t, size, currentColor);
				renderer.pop();
			}
		}
	}
	else
	{
		// draw the background and the frame
		Renderer& renderer = Renderer::inst();
		const bool lit = pushed && mouseOver;
		const Vec4f top = lit ? Vec4f(0.9f, 0.9f, 0.9f, 1.0f) : Vec4f(0.75f, 0.75f, 0.75f, 1.0f);
		const Vec4f bottom = lit ? Vec4f(0.8f, 0.8f, 0.8f, 1.0f) : Vec4f(0.65f, 0.65f, 0.65f, 1.0f);
		const Vec2f s = static_cast<Vec2f>(size);
		const Vec2f corners[4] = {Vec2f(0.0f, 0.0f), Vec2f(s.x, 0.0f), s, Vec2f(0.0f, s.y)};
		const Vec4f colors[4] = {top, top, bottom, bottom};
		renderer.quad(corners, colors);
		renderer.hairlineRect(Vec2f(0.0f, 0.0f), s, Vec4f(0.0f, 0.0f, 0.0f, 1.0f));
	}

	// write the title
	Vec2i dim;
	std::string title = localizeString(this->title);
	p_font->measureText(title, &dim, 0);

	if(style == 0)
	{
		p_font->renderText(title, (size - dim) / 2 + Vec2i(0, offset), active ? Vec4f(1.0f, 1.0f, 1.0f, 1.0f) : Vec4f(0.5f, 0.5f, 0.5f, 1.0f));

		if(p_image)
		{
			// render the image
			Engine::inst().renderSprite(p_image, Vec2i(0, offset), positionOnTexture, size, Vec4f(1.0f));
		}
	}
	else
	{
		// Two pixels below the image, not below the element. The two are not
		// the same: the element has a border around the image, and counting
		// that border in pushes every caption in the main menu down by
		// exactly that border.
		p_font->renderText(title, Vec2i((size.x - dim.x) / 2, size.y - imageInset + 2), active ? currentColor : Vec4f(0.5f, 0.5f, 0.5f, 1.0f));
	}
}

void GUI_Button::onUpdate()
{
	currentColor = 0.85f * currentColor + 0.15f * (mouseOver ? hoverColor : stdColor);
	currentScaling = 0.85f * currentScaling + 0.15f * (mouseOver ? hoverScaling : stdScaling);

	// Resolving it at load time is not enough: the donate button carries
	// $MM_DONATE_BUTTON_FILENAME, and a player who switches the language in
	// the options would otherwise see the old button until the next start.
	// Titles do the same, only at draw time. The texture is still requested
	// only when a different name really comes out.
	if(!rawImageFilename.empty())
	{
		const std::string wanted = localizeString(rawImageFilename);
		if(wanted != imageFilename) setImageFilename(wanted);
	}
}

void GUI_Button::onMouseDown(const Vec2i& position,
							 int buttons)
{
	if(active && (buttons & 1)) pushed = true;
}

void GUI_Button::onMouseUp(const Vec2i& position,
						   int buttons)
{
	if(pushed && (buttons & 1))
	{
		if(mouseOver) click();
		pushed = false;
	}
}

void GUI_Button::onMouseEnter(int buttons)
{
	mouseOver = true;
}

void GUI_Button::onMouseLeave(int buttons)
{
	mouseOver = false;
}

void GUI_Button::click()
{
	// fire the clicked signal
	clicked(this);
}

void GUI_Button::readAttributes(TiXmlElement* p_element)
{
	TiXmlElement* e = p_element->FirstChildElement("Title");
	if(e)
	{
		const char* p_title = e->GetText();
		setTitle(p_title ? p_title : "");
	}

	e = p_element->FirstChildElement("Image");
	if(e)
	{
		const char* p_imageFilename = e->GetText();
		if(p_imageFilename) setRawImageFilename(p_imageFilename);

		e->QueryIntAttribute("u", &positionOnTexture.x);
		e->QueryIntAttribute("v", &positionOnTexture.y);

		e->QueryIntAttribute("u2", &clickedPositionOnTexture.x);
		e->QueryIntAttribute("v2", &clickedPositionOnTexture.y);
	}

	e = p_element->FirstChildElement("ExtendedStyle");
	if(e)
	{
		style = 1;
		e->QueryIntAttribute("inset", &imageInset);
	}
}

void GUI_Button::setImageFilename(const std::string& imageFilename)
{
	if(p_image) p_image->release();
	this->imageFilename = imageFilename;
	p_image = Manager<Texture>::inst().request(imageFilename);
}

void GUI_Button::setRawImageFilename(const std::string& rawImageFilename)
{
	this->rawImageFilename = rawImageFilename;
	setImageFilename(localizeString(rawImageFilename));
}