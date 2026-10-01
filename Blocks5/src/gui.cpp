#include "pch.h"
#include "gui.h"
#include "gui_element.h"
#include "font.h"
#include "texture.h"
#include "engine.h"

GUI::GUI()
{
	initialized = false;
	p_root = 0;
	p_font = 0;
	p_toolTipFont = 0;
	p_skin = 0;
	p_elementAtCursor = 0;
	p_oldElementAtCursor = 0;
	p_focusElement = 0;
	p_oldFocusElement = 0;
	p_mouseDownElement = 0;
	keyRepeat = false;
}

GUI::~GUI()
{
	exit();
}

bool GUI::isKeyRepeat() const
{
	return keyRepeat;
}

bool GUI::init()
{
	if(initialized) return false;

	// create the root element
	p_root = new GUI_Element("ROOT", 0, Vec2i(0, 0), Engine::inst().getScreenSize());

	// load the fonts
	p_font = Manager<Font>::inst().request("font.xml");
	if(!p_font) return false;
	Font::Options options = p_font->getOptions();
	options.shadows = 1;
	p_font->setOptions(options);

	p_toolTipFont = Manager<Font>::inst().request("tooltip_font.xml");
	if(!p_toolTipFont) return false;
	options = p_toolTipFont->getOptions();
	options.shadows = 1;
	p_toolTipFont->setOptions(options);

	// load the skin
	p_skin = Manager<Texture>::inst().request("gui.png");

	texID = 0;
	setOpacity(0.85f);

	cursorPos = oldCursorPos = Engine::inst().getCursorPosition();
	oldRawCursorPos = Engine::inst().getRawCursorPosition();
	p_elementAtCursor = p_oldElementAtCursor = p_root;
	p_focusElement = 0;
	p_oldFocusElement = 0;
	p_mouseDownElement = 0;
	noMoveCounter = 0;

	initialized = true;

	return true;
}

void GUI::exit()
{
	if(!initialized) return;

	// delete the root element (and with it every element)
	delete p_root;
	p_root = 0;

	// delete the texture
	Renderer::inst().deleteTexture(texID);
	texID = 0;

	// release the skin and the fonts
	if(p_skin) p_skin->release();
	p_font->release();
	p_toolTipFont->release();
	p_skin = 0;
	p_font = 0;
	p_toolTipFont = 0;

	initialized = false;
}

void GUI::render()
{
	if(opacity == 1.0f || opacity == 0.0f) return;

	// Drawn onto a cleared frame and copied off it, so that display() can
	// put the whole of it back over the game at the chosen opacity.
	Renderer& renderer = Renderer::inst();
	renderer.clear(Vec4f(0.0f, 0.0f, 0.0f, 0.0f));
	renderer.setBlend(BM_NORMAL);

	GUI_Element::numElementsRendered = 0;
	p_root->render();
	renderToolTip();

	if(GUI_Element::numElementsRendered) Engine::inst().captureFrame(texID);
}

void GUI::renderToolTip()
{
	if(p_elementAtCursor && noMoveCounter >= 20)
	{
		if(!p_elementAtCursor->getToolTip().empty())
		{
			std::string toolTip = localizeString(p_elementAtCursor->getToolTip());
			Vec2i dim;
			p_toolTipFont->measureText(toolTip, &dim, 0);
			Vec2i ttPos = cursorPos + Vec2i(0, 20);
			Vec2i ttDim = dim + Vec2i(6, 7);

			const Vec2i& screenSize = Engine::inst().getScreenSize();
			if(ttPos.x + ttDim.x > screenSize.x - 1) ttPos.x = screenSize.x - ttDim.x - 1;
			if(ttPos.y + ttDim.y > screenSize.y) ttPos.y = screenSize.y - ttDim.y;

			Renderer& renderer = Renderer::inst();
			const Vec2f min = static_cast<Vec2f>(ttPos), max = static_cast<Vec2f>(ttPos + ttDim);
			renderer.rect(min, max, Vec4f(1.0f, 1.0f, 0.5f, 0.9f));
			renderer.hairlineRect(min, max, Vec4f(0.0f, 0.0f, 0.0f, 0.9f));

			p_toolTipFont->renderText(toolTip, ttPos + Vec2i(3, 3), Vec4f(1.0f));
		}
	}
}

void GUI::display()
{
	if(opacity == 0.0f || (opacity != 1.0f && !GUI_Element::numElementsRendered)) return;

	if(opacity == 1.0f)
	{
		Renderer::inst().setBlend(BM_NORMAL);

		GUI_Element::numElementsRendered = 0;
		p_root->render();
		renderToolTip();
	}
	else
	{
		// The copy render() took, over the game; its texel scale puts the
		// coordinates in pixels.
		Engine& engine = Engine::inst();
		const Vec2f screenSize = static_cast<Vec2f>(engine.getScreenSize());
		const Vec2f corners[4] = {Vec2f(0.0f, 0.0f), Vec2f(screenSize.x, 0.0f), screenSize, Vec2f(0.0f, screenSize.y)};
		Renderer::inst().quad(RenderState(engine.getFrameCopyRef(texID), BM_NORMAL), corners, corners,
							  Vec4f(1.0f, 1.0f, 1.0f, opacity));
	}
}

void GUI::update()
{
	Engine& engine = Engine::inst();

	p_root->update();

	const int buttonsDown = (engine.isButtonDown(1) ? 1 : 0) | (engine.isButtonDown(3) ? 2 : 0);
	const int buttonsPressed = (engine.wasButtonPressed(1) ? 1 : 0) | (engine.wasButtonPressed(3) ? 2 : 0);
	const int buttonsReleased = (engine.wasButtonReleased(1) ? 1 : 0) | (engine.wasButtonReleased(3) ? 2 : 0);

	// update the mouse position and the element under it
	oldCursorPos = cursorPos;
	cursorPos = engine.getCursorPosition();
	Vec2i cursorMovement = cursorPos - oldCursorPos;

	// A mouse-move event has to mean the mouse moved. cursorPos passes through
	// the CRT filter's barrel distortion, so dragging its curvature slider
	// moves the cursor in the picture under a still hand; that phantom move
	// would set a new value, hence a new curvature, and the slider would flip
	// between two values at the logic rate. So both are demanded: the cursor
	// moved in the window, and its game-space position landed on another pixel.
	const Vec2i rawCursorPos = engine.getRawCursorPosition();
	const bool cursorMoved = rawCursorPos != oldRawCursorPos && cursorPos != oldCursorPos;
	oldRawCursorPos = rawCursorPos;

	// Before the click handling: a finger lands with no move before it, so
	// cursor and press arrive in the same tick, and the press must go to what
	// is under the finger now rather than to what was under the cursor before.
	p_oldElementAtCursor = p_elementAtCursor;
	p_elementAtCursor = p_root->getElementAt(cursorPos);

	Vec2i relCursorPos;
	if(p_elementAtCursor) relCursorPos = cursorPos - p_elementAtCursor->getAbsPosition();
	else relCursorPos = cursorPos;
	p_oldFocusElement = p_focusElement;

	// Did the element change?
	if(p_elementAtCursor != p_oldElementAtCursor)
	{
		// tell the elements about it
		if(p_oldElementAtCursor) p_oldElementAtCursor->onMouseLeave(buttonsDown);
		if(p_elementAtCursor) p_elementAtCursor->onMouseEnter(buttonsDown);
	}

	if(p_elementAtCursor && p_elementAtCursor->isReallyVisible())
	{
		// Did a click happen?
		if(buttonsPressed)
		{
			p_focusElement = p_elementAtCursor;
			if(p_elementAtCursor->isActive()) p_elementAtCursor->bringToFront();
			p_elementAtCursor->onMouseDown(relCursorPos, buttonsPressed);
			/* if(buttonsPressed & 1) */ p_mouseDownElement = p_elementAtCursor;
		}

		if(buttonsDown & 1) noMoveCounter = 10;

		if(buttonsReleased)
		{
			p_elementAtCursor->onMouseUp(relCursorPos, buttonsReleased);

			if(p_mouseDownElement &&
			   p_mouseDownElement != p_elementAtCursor)
			{
				// inform the element under the cursor at the time of the press
				p_mouseDownElement->onMouseUp(cursorPos - p_mouseDownElement->getAbsPosition(), buttonsReleased);
			}

			p_mouseDownElement = 0;
		}

		// Did the mouse move?
		if(cursorMoved)
		{
			// tell the element about it
			p_elementAtCursor->onMouseMove(relCursorPos, cursorMovement, buttonsDown);

			if(p_mouseDownElement &&
			   p_mouseDownElement != p_elementAtCursor)
			{
				// inform the element under the cursor at the time of the press
				p_mouseDownElement->onMouseMove(cursorPos - p_mouseDownElement->getAbsPosition(), cursorMovement, buttonsDown);
			}
		}
	}

	// Keyboard events?
	SDL_KeyboardEvent event;
	while(engine.getKeyEvent(&event, &keyRepeat))
	{
		if(p_focusElement) p_focusElement->onKeyEvent(event);
	}
	keyRepeat = false;

	// Mouse wheel?
	if(p_elementAtCursor)
	{
		int wheel = 0;
		if(engine.wasButtonPressed(SDL_BUTTON_WHEELUP)) wheel = -1;
		else if(engine.wasButtonPressed(SDL_BUTTON_WHEELDOWN)) wheel = 1;
		if(wheel) p_elementAtCursor->onMouseWheel(wheel);
	}

	if(!cursorMoved) noMoveCounter = min<uint>(20, noMoveCounter + 1);
	else if(p_elementAtCursor != p_oldElementAtCursor && noMoveCounter) noMoveCounter--;

	if(p_elementAtCursor && p_elementAtCursor->getToolTip().empty() && noMoveCounter) --noMoveCounter;
}

void GUI::renderFrame(const Vec2i& targetPosition,
					  const Vec2i& size,
					  const Vec2i& positionOnTexture)
{
	if(!p_skin) return;


	Vec2i firstSize;
	Vec2i lastSize;

	if(size.x < 32)
	{
		firstSize.x = size.x / 2;
		lastSize.x = size.x - firstSize.x;
	}
	else
	{
		firstSize.x = lastSize.x = 16;
	}

	if(size.y < 32)
	{
		firstSize.y = size.y / 2;
		lastSize.y = size.y - firstSize.y;
	}
	else
	{
		firstSize.y = lastSize.y = 16;
	}

	Vec2i fillSize = size - Vec2i(32, 32);
	fillSize.x = max(0, fillSize.x);
	fillSize.y = max(0, fillSize.y);
	Vec2i numFillTiles((fillSize.x + 15) / 16, (fillSize.y + 15) / 16);
	Vec2i numTiles(2 + numFillTiles.x, 2 + numFillTiles.y);
	Vec2i lastFillTileSize = Vec2i(16, 16) - (numFillTiles * 16 - fillSize);

	// The tiles as one array of quads, uv in the skin's texels.
	std::vector<QuadVertex> quads;
	quads.reserve(numTiles.x * numTiles.y * 4);

	Vec2i cursor = targetPosition;
	for(int y = 0; y < numTiles.y; y++)
	{
		Vec2i tileSize;
		tileSize.y = 16;
		Vec2i texCoords;
		texCoords.y = positionOnTexture.y + 16;

		if(y == 0)
		{
			// first tile on the y axis
			tileSize.y = firstSize.y;
			texCoords.y = positionOnTexture.y;
		}
		else if(y == numTiles.y - 1)
		{
			// last tile on the y axis
			tileSize.y = lastSize.y;
			texCoords.y = positionOnTexture.y + 32 + (16 - lastSize.y);
		}
		else if(y == numTiles.y - 2 && numTiles.y >= 3)
		{
			// last fill tile on the y axis
			tileSize.y = lastFillTileSize.y;
		}

		for(int x = 0; x < numTiles.x; x++)
		{
			tileSize.x = 16;
			texCoords.x = positionOnTexture.x + 16;

			if(x == 0)
			{
				// first tile on the x axis
				tileSize.x = firstSize.x;
				texCoords.x = positionOnTexture.x;
			}
			else if(x == numTiles.x - 1)
			{
				// last tile on the x axis
				tileSize.x = lastSize.x;
				texCoords.x = positionOnTexture.x + 32 + (16 - lastSize.x);
			}
			else if(x == numTiles.x - 2 && numTiles.x >= 3)
			{
				// last fill tile on the x axis
				tileSize.x = lastFillTileSize.x;
			}

			// Render the tile; its edges and texels are whole numbers, added up as such.
			const float x0 = static_cast<float>(cursor.x), y0 = static_cast<float>(cursor.y);
			const float x1 = static_cast<float>(cursor.x + tileSize.x), y1 = static_cast<float>(cursor.y + tileSize.y);
			const float u0 = static_cast<float>(texCoords.x), v0 = static_cast<float>(texCoords.y);
			const float u1 = static_cast<float>(texCoords.x + tileSize.x), v1 = static_cast<float>(texCoords.y + tileSize.y);
			quads.push_back(QuadVertex(x0, y0, u0, v0));
			quads.push_back(QuadVertex(x1, y0, u1, v0));
			quads.push_back(QuadVertex(x1, y1, u1, v1));
			quads.push_back(QuadVertex(x0, y1, u0, v1));

			cursor.x += tileSize.x;
		}

		cursor.y += tileSize.y;
		cursor.x = targetPosition.x;
	}

	Renderer& renderer = Renderer::inst();
	renderer.setTexture(p_skin->ref());
	renderer.quads(renderer.state(), &quads[0], static_cast<uint>(quads.size()), Vec4f(1.0f, 1.0f, 1.0f, 1.0f));
}

GUI_Element* GUI::getElement(const std::string& fullName)
{
	if(fullName.empty()) return p_root;

	GUI_Element* p_element = p_root;
	std::string elementName = "";
	for(uint i = 0; i <= static_cast<uint>(fullName.length()); i++)
	{
		char c;
		if(i == static_cast<uint>(fullName.length())) c = '.';
		else c = fullName[i];
		if(c == '.')
		{
			const std::list<GUI_Element*>& children = p_element->getChildren();
			for(std::list<GUI_Element*>::const_iterator j = children.begin(); j != children.end(); ++j)
			{
				if((*j)->getName() == elementName)
				{
					p_element = *j;
					elementName = "";
					break;
				}
			}

			if(!elementName.empty()) return 0;
		}
		else
		{
			elementName.append(1, c);
		}
	}

	return p_element;
}

GUI_Element* GUI::operator [] (const std::string& fullName)
{
	return getElement(fullName);
}

GUI_Element* GUI::getRoot()
{
	return p_root;
}

Font* GUI::getFont()
{
	return p_font;
}

Font* GUI::getToolTipFont()
{
	return p_toolTipFont;
}

float GUI::getOpacity() const
{
	return opacity;
}

void GUI::setOpacity(float opacity)
{
	opacity = clamp(opacity, 0.0f, 1.0f);
	this->opacity = opacity;

	if(opacity == 1.0f && texID)
	{
		// delete the texture
		Renderer::inst().deleteTexture(texID);
		texID = 0;
	}

	if(opacity != 1.0f && !texID)
	{
		// create the texture, with alpha: the GUI is drawn onto nothing
		texID = Engine::inst().createFrameCopyTexture(true, true);
	}
}

Texture* GUI::getSkin()
{
	return p_skin;
}

void GUI::setSkin(Texture* p_skin)
{
	if(this->p_skin) this->p_skin->release();
	this->p_skin = p_skin;
}

const Vec2i& GUI::getCursorPos() const
{
	return cursorPos;
}

GUI_Element* GUI::getFocusElement()
{
	return p_focusElement;
}

void GUI::setFocusElement(GUI_Element* p_element)
{
	if(!p_element) p_element = p_root;

	if(p_focusElement == p_element) return;
	p_focusElement = p_element;
}

GUI_Element* GUI::getOldFocusElement()
{
	return p_oldFocusElement;
}

GUI_Element* GUI::getMouseDownElement()
{
	return p_mouseDownElement;
}

const std::string& GUI::getClipboard() const
{
	return clipboard;
}

void GUI::setClipboard(const std::string& clipboard)
{
	this->clipboard = clipboard;
}