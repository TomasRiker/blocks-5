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
	setOpacity(0.85);

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
	GL::deleteTexture(texID);
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
	if(opacity == 1.0 || opacity == 0.0) return;

	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);

	glLineWidth(1.0f);
	glDisable(GL_LINE_SMOOTH);
	Engine::inst().setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);

	GUI_Element::numElementsRendered = 0;
	p_root->render();
	renderToolTip();

	glEnable(GL_LINE_SMOOTH);

	if(GUI_Element::numElementsRendered)
	{
		Engine& engine = Engine::inst();
		const Vec2i& screenSize = engine.getScreenSize();
		const Vec2i& screenPow2Size = engine.getScreenPow2Size();
		GL::bindTexture(texID, engine.getScreenTexelScale());
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, screenPow2Size.y - screenSize.y, 0, 0, screenSize.x, screenSize.y);
	}
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

			glBegin(GL_QUADS);
			glColor4d(1.0, 1.0, 0.5, 0.9);
			glVertex2i(ttPos.x, ttPos.y);
			glVertex2i(ttPos.x + ttDim.x, ttPos.y);
			glVertex2i(ttPos.x + ttDim.x, ttPos.y + ttDim.y);
			glVertex2i(ttPos.x, ttPos.y + ttDim.y);
			glEnd();
			glBegin(GL_LINE_LOOP);
			glColor4d(0.0, 0.0, 0.0, 0.9);
			glVertex2i(ttPos.x, ttPos.y);
			glVertex2i(ttPos.x + ttDim.x, ttPos.y);
			glVertex2i(ttPos.x + ttDim.x, ttPos.y + ttDim.y);
			glVertex2i(ttPos.x, ttPos.y + ttDim.y);
			glEnd();

			p_toolTipFont->renderText(toolTip, ttPos + Vec2i(3, 3), Vec4d(1.0));
		}
	}
}

void GUI::display()
{
	if(opacity == 0.0 || (opacity != 1.0 && !GUI_Element::numElementsRendered)) return;

	if(opacity == 1.0)
	{
		glLineWidth(1.0f);
		glDisable(GL_LINE_SMOOTH);
		Engine::inst().setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);

		GUI_Element::numElementsRendered = 0;
		p_root->render();
		renderToolTip();

		glEnable(GL_LINE_SMOOTH);
	}
	else
	{
		GL::setTexturing(true);

		// The scale is what puts the texture coordinates below in pixels.
		GL::bindTexture(texID, Engine::inst().getScreenTexelScale());

		const Vec2i& screenSize = Engine::inst().getScreenSize();
		glBegin(GL_QUADS);
		glColor4d(1.0, 1.0, 1.0, opacity);
		glTexCoord2i(0, 0);
		glVertex2i(0, 0);
		glTexCoord2i(screenSize.x, 0);
		glVertex2i(screenSize.x, 0);
		glTexCoord2i(screenSize.x, screenSize.y);
		glVertex2i(screenSize.x, screenSize.y);
		glTexCoord2i(0, screenSize.y);
		glVertex2i(0, screenSize.y);
		glEnd();

		GL::setTexturing(false);
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

	// A mouse-move event has to mean the mouse moved. cursorPos comes from
	// getCursorPosition() and therefore through the CRT filter's barrel
	// distortion: dragging its curvature slider shifts the mapping under the
	// hand holding it, and the cursor travels in the picture with nothing
	// having stirred. Such a phantom move would otherwise become a new slider
	// value, that value a new curvature, and the slider would flip back and
	// forth between two values at the logic rate. Demand both, then: the mouse
	// must have moved, and it must have landed on another pixel in the process.
	const Vec2i rawCursorPos = engine.getRawCursorPosition();
	const bool cursorMoved = rawCursorPos != oldRawCursorPos && cursorPos != oldCursorPos;
	oldRawCursorPos = rawCursorPos;

	// The element under the cursor belongs here, before the click handling,
	// not at the end of the function: a finger lands with no move before it,
	// and cursor and press therefore arrive in the same tick. Were it computed
	// further down, the press would still go to whatever lay under the cursor
	// before.
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

	p_skin->bind();

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

	glBegin(GL_QUADS);
	glColor4d(1.0, 1.0, 1.0, 1.0);

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

			// render the tile
			glTexCoord2i(texCoords.x, texCoords.y);
			glVertex2i(cursor.x, cursor.y);
			glTexCoord2i(texCoords.x + tileSize.x, texCoords.y);
			glVertex2i(cursor.x + tileSize.x, cursor.y);
			glTexCoord2i(texCoords.x + tileSize.x, texCoords.y + tileSize.y);
			glVertex2i(cursor.x + tileSize.x, cursor.y + tileSize.y);
			glTexCoord2i(texCoords.x, texCoords.y + tileSize.y);
			glVertex2i(cursor.x, cursor.y + tileSize.y);

			cursor.x += tileSize.x;
		}

		cursor.y += tileSize.y;
		cursor.x = targetPosition.x;
	}

	glEnd();

	p_skin->unbind();
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

double GUI::getOpacity() const
{
	return opacity;
}

void GUI::setOpacity(double opacity)
{
	opacity = clamp(opacity, 0.0, 1.0);
	this->opacity = opacity;

	if(opacity == 1.0 && texID)
	{
		// delete the texture
		GL::deleteTexture(texID);
		texID = 0;
	}

	if(opacity != 1.0 && !texID)
	{
		// create the texture
		glGenTextures(1, &texID);
		GL::bindTexture(texID, Engine::inst().getScreenTexelScale());
		const Vec2i screenPow2Size = Engine::inst().getScreenPow2Size();
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, screenPow2Size.x, screenPow2Size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
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