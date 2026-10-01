#include "pch.h"
#include "gui_button.h"
#include "engine.h"
#include "texture.h"

namespace
{
	// The flashing button, for the author to tune: how long one pulse takes in
	// logic ticks (50 to the second), and the colour the frame takes on at its
	// height. The skin's frame is nearly white, so the colour is what it
	// turns; alpha is how far, 0 leaving the frame as it is and 1 all the way.
	// A longer period is calmer, a lower alpha fainter.
	const uint FLASH_PERIOD_TICKS = 50;
	const Vec4f FLASH_COLOR(1.0f, 0.7f, 0.15f, 0.85f);
	const float TWO_PI = 6.28318531f;
}

IMPL_CTOR(GUI_Button)
{
	title = "Button";
	pushed = false;
	mouseOver = false;
	flashing = false;
	flashTicks = 0;

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
			const Vec2i frame = pushed && mouseOver ? Vec2i(48, 96) : Vec2i(0, 96);
			gui.renderFrame(Vec2i(0, 0), size, frame);
			offset = -1;

			if(flashing)
			{
				// The same frame once more on top, tinted, its opacity a pulse
				// that swells from nothing to FLASH_COLOR's alpha and back.
				// Tinted and blended rather than added, since adding to a frame
				// this light only reaches white.
				const float phase = static_cast<float>(flashTicks % FLASH_PERIOD_TICKS) /
									static_cast<float>(FLASH_PERIOD_TICKS);
				const float pulse = 0.5f - 0.5f * cosf(TWO_PI * phase);
				gui.renderFrame(Vec2i(0, 0), size, frame,
								Vec4f(FLASH_COLOR.x, FLASH_COLOR.y, FLASH_COLOR.z, FLASH_COLOR.w * pulse));
			}
		}
		else
		{
			// With no image the button draws nothing and is still clickable:
			// menu.xml lays such buttons over text that belongs to the
			// background picture.
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
	const std::string title = localizeString(this->title);
	Vec2i dim;
	p_font->measureText(title, &dim, 0);

	if(style == 0)
	{
		renderTitle(title, (size.y - dim.y) / 2 + offset, active ? Vec4f(1.0f, 1.0f, 1.0f, 1.0f) : Vec4f(0.5f, 0.5f, 0.5f, 1.0f));

		if(p_image)
		{
			// render the image
			Engine::inst().renderSprite(p_image, Vec2i(0, offset), positionOnTexture, size, Vec4f(1.0f));
		}
	}
	else
	{
		// Two pixels below the image, which ends imageInset above the
		// element's bottom edge.
		renderTitle(title, size.y - imageInset + 2, active ? currentColor : Vec4f(0.5f, 0.5f, 0.5f, 1.0f));
	}
}

void GUI_Button::renderTitle(const std::string& localized,
							 int top,
							 const Vec4f& color)
{
	// The step from one line to the next that Font::buildText() takes.
	const int lineStep = static_cast<int>(p_font->getOptions().lineSpacing * static_cast<float>(p_font->getLineHeight()));

	size_t begin = 0;
	for(int line = 0; ; line++)
	{
		const size_t end = localized.find_first_of("\n\xB6", begin);
		const std::string text(localized, begin, end == std::string::npos ? std::string::npos : end - begin);

		Vec2i dim;
		p_font->measureText(text, &dim, 0);
		p_font->renderText(text, Vec2i((size.x - dim.x) / 2, top + line * lineStep), color);

		if(end == std::string::npos) break;
		begin = end + 1;
	}
}

Vec2i GUI_Button::measureTitle()
{
	Vec2i dim;
	p_font->measureText(localizeString(title), &dim, 0);
	return dim;
}

void GUI_Button::setFlashing(bool flashing)
{
	this->flashing = flashing;
}

void GUI_Button::onUpdate()
{
	// Counted here rather than read off a clock, so that the pulse starts from
	// nothing whenever the flashing does.
	flashTicks = flashing ? flashTicks + 1 : 0;

	currentColor = 0.85f * currentColor + 0.15f * (mouseOver ? hoverColor : stdColor);
	currentScaling = 0.85f * currentScaling + 0.15f * (mouseOver ? hoverScaling : stdScaling);

	// Resolved every tick, as titles are at draw time: the donate button's
	// $MM_DONATE_BUTTON_FILENAME changes with the language. The texture is
	// requested again only when the name does.
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
	// A deactivated button does nothing, whoever clicks it: a press that began
	// while it was active can be released after, as when the level selection
	// steps on to a locked level under a held Play, and Return reaches a
	// submit button through an edit box or a list.
	if(!active) return;

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