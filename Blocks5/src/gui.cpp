#include "pch.h"
#include "gui.h"
#include "gui_element.h"
#include "font.h"
#include "texture.h"
#include "engine.h"

namespace
{
	// How far a finger reaches, and how much nearer than any other the
	// element it meant has to be, in reference pixels
	// (Engine::getReferencePixelScale): a physical length, the same on a
	// phone and on a Windows tablet. 16 lifts an 18-pixel button on a phone
	// in landscape to about the 44 CSS pixels the accessibility guidance asks
	// for a finger. Both are feel, for the author to tune on a device: a
	// larger reach catches wider misses, a larger margin leaves more taps
	// between two targets to nobody.
	const float TOUCH_RADIUS = 16.0f;
	const float TOUCH_MARGIN = 4.0f;

	// The rings the search walks out on and the spacing of the points on
	// each, in game pixels. Below half the narrowest target in the tree, a
	// 14-pixel checkbox, so none can lie between two points unseen.
	const float TOUCH_STEP = 2.0f;

	const float TWO_PI = 6.28318531f;

	// Whether a press at this point, in screen coordinates, does something
	// where it lands.
	bool takesPress(GUI_Element* p_element, const Vec2i& point)
	{
		return p_element->isClickTarget(point - p_element->getAbsPosition());
	}

	// The points of one ring of the search, staggered from ring to ring so
	// that two rings do not line their points up along the same rays.
	void ringPoints(const Vec2i& centre, float radius, int ring, std::vector<Vec2i>& points)
	{
		points.clear();
		const int count = max(8, static_cast<int>(ceilf(TWO_PI * radius / TOUCH_STEP)));
		const float stagger = (ring & 1) ? 0.5f : 0.0f;
		for(int k = 0; k < count; k++)
		{
			const float angle = TWO_PI * (static_cast<float>(k) + stagger) / static_cast<float>(count);
			points.push_back(centre + Vec2i(static_cast<int>(floorf(radius * cosf(angle) + 0.5f)),
											static_cast<int>(floorf(radius * sinf(angle) + 0.5f))));
		}
	}
}

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
	p_fingerElement = 0;
	fingerOffset = Vec2i(0, 0);
	fingerHolds = false;
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
	p_fingerElement = 0;
	fingerOffset = Vec2i(0, 0);
	fingerHolds = false;
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

	// A finger's press goes to what it meant (pickTouchTarget), moved by as
	// much as it took to reach it, and so does the rest of the gesture: the
	// element it went to is told every position until the finger lifts moved
	// by that offset (pointFor), so a slider taken hold of beside its bar
	// follows the finger as it would a mouse that had pressed on it. And that
	// element stays under the finger while the finger is within reach of it:
	// a button fires on release only while it believes the cursor is on it,
	// and a finger wobbling off a small one, or one it only came near, would
	// cancel its own press. Out of reach, or hidden, it lets go, as a mouse
	// dragged off a button does. The engine's cursor stays where the finger
	// is.
	if(buttonsPressed)
	{
		Vec2i landing = cursorPos;
		p_fingerElement = engine.wasFingerPress() ? pickTouchTarget(cursorPos, &landing) : 0;
		fingerOffset = landing - cursorPos;
		fingerHolds = p_fingerElement != 0;
	}
	else if(fingerHolds &&
			(!p_fingerElement->isReallyVisible() || (cursorMoved && !fingerReaches(p_fingerElement, cursorPos))))
	{
		fingerHolds = false;
	}

	p_elementAtCursor = fingerHolds ? p_fingerElement : p_root->getElementAt(cursorPos);

	Vec2i relCursorPos;
	if(p_elementAtCursor) relCursorPos = pointFor(p_elementAtCursor) - p_elementAtCursor->getAbsPosition();
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
				p_mouseDownElement->onMouseUp(pointFor(p_mouseDownElement) - p_mouseDownElement->getAbsPosition(), buttonsReleased);
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
				p_mouseDownElement->onMouseMove(pointFor(p_mouseDownElement) - p_mouseDownElement->getAbsPosition(), cursorMovement, buttonsDown);
			}
		}
	}

	// Off the glass, the gesture is over: from the next tick on, what lies
	// under the cursor is under it again. A tap whose press and release came
	// in one tick got both above.
	if(!buttonsDown)
	{
		p_fingerElement = 0;
		fingerHolds = false;
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

// A finger is not a point. Searched outward from the point in rings, asking
// at every point of a ring what a press there would hit - getElementAt(), so
// that z-order, a pane in front, a modal one over everything, a hidden
// element and every overridden hit shape come out exactly as for a mouse. Of
// what takes a press there, the nearest wins: by distance, not by how much of
// the disc it covers, which a large list would win against a small button
// every time. A label and the box it labels are one target, so a finger
// between the two is not torn between them.
GUI_Element* GUI::pickTouchTarget(const Vec2i& point, Vec2i* p_landing)
{
	*p_landing = point;
	if(!p_root) return 0;

	GUI_Element* p_exact = p_root->getElementAt(point);

	// Right on something that takes a press: that, enabled or not. A tap on a
	// greyed-out button must not slide onto the one beside it, and one on the
	// level stays on the level - the search is for a near miss and changes
	// no tap that hit.
	if(!p_exact || takesPress(p_exact, point)) return p_exact;

	const float scale = Engine::inst().getReferencePixelScale();
	const float radius = TOUCH_RADIUS * scale;
	const float margin = TOUCH_MARGIN * scale;

	// The nearest point found of each target, keyed on what a press there
	// ends up with.
	struct Candidate
	{
		GUI_Element* p_receiver;
		GUI_Element* p_element;
		float distance;
		Vec2i landing;
	};
	std::vector<Candidate> candidates;
	float nearest = -1.0f;

	std::vector<Vec2i> points;
	for(int ring = 1; ; ring++)
	{
		const float ringRadius = min(radius, static_cast<float>(ring) * TOUCH_STEP);

		// Past the nearest find by more than the margin, nothing can change
		// the answer: anything found from here on is no rival to it.
		if(nearest >= 0.0f && ringRadius > nearest + margin) break;

		ringPoints(point, ringRadius, ring, points);
		for(std::vector<Vec2i>::const_iterator i = points.begin(); i != points.end(); ++i)
		{
			GUI_Element* p_hit = p_root->getElementAt(*i);
			if(!p_hit || !p_hit->isActive() || !takesPress(p_hit, *i)) continue;

			GUI_Element* p_receiver = p_hit->getPressReceiver();
			if(!p_receiver->isActive() || !p_receiver->isReallyVisible()) continue;

			const Vec2i offset = *i - point;
			const float distance = sqrtf(static_cast<float>(offset.x * offset.x + offset.y * offset.y));

			std::vector<Candidate>::iterator c = candidates.begin();
			while(c != candidates.end() && c->p_receiver != p_receiver) ++c;
			if(c == candidates.end())
			{
				Candidate candidate = { p_receiver, p_hit, distance, *i };
				candidates.push_back(candidate);
			}
			else if(distance < c->distance)
			{
				c->p_element = p_hit;
				c->distance = distance;
				c->landing = *i;
			}

			if(nearest < 0.0f || distance < nearest) nearest = distance;
		}

		if(ringRadius >= radius) break;
	}

	if(candidates.empty()) return p_exact;

	const Candidate* p_first = 0;
	const Candidate* p_second = 0;
	for(std::vector<Candidate>::const_iterator c = candidates.begin(); c != candidates.end(); ++c)
	{
		if(!p_first || c->distance < p_first->distance)
		{
			p_second = p_first;
			p_first = &*c;
		}
		else if(!p_second || c->distance < p_second->distance) p_second = &*c;
	}

	// Two about as near: which one the finger meant is a guess, and a guess
	// is worse than the dead tap a mouse would have made there.
	if(p_second && p_second->distance < p_first->distance + margin) return p_exact;

	// The rings find the nearest point only as closely as their spacing
	// allows. Where the element is hit at the nearest point of its rectangle,
	// right across from the finger, and takes a press there, that is the
	// exact answer: a slider or an edit box pressed from beside it is pressed
	// where the finger is along it, not a ring step to one side. Not where
	// that point lies further off, as a toggle's rectangle does from a finger
	// by its caption, nor where it does nothing - a window's body, the
	// editor's toolbar - since what the rings found there is the title bar or
	// the level, and that is where the press goes.
	GUI_Element* p_element = p_first->p_element;
	*p_landing = p_first->landing;
	const Vec2i corner = p_element->getAbsPosition();
	const Vec2i size = p_element->getSize();
	if(size.x > 0 && size.y > 0)
	{
		const int right = corner.x + size.x - 1;
		const int bottom = corner.y + size.y - 1;
		const Vec2i across(clamp(point.x, corner.x, right), clamp(point.y, corner.y, bottom));
		const Vec2i offset = across - point;
		const float distance = sqrtf(static_cast<float>(offset.x * offset.x + offset.y * offset.y));
		if(distance <= p_first->distance && p_root->getElementAt(across) == p_element && takesPress(p_element, across))
		{
			*p_landing = across;
		}
	}

	return p_element;
}

bool GUI::fingerReaches(GUI_Element* p_element, const Vec2i& point)
{
	if(!p_root) return false;
	if(p_root->getElementAt(point) == p_element) return true;

	const float radius = TOUCH_RADIUS * Engine::inst().getReferencePixelScale();
	std::vector<Vec2i> points;
	for(int ring = 1; ; ring++)
	{
		const float ringRadius = min(radius, static_cast<float>(ring) * TOUCH_STEP);
		ringPoints(point, ringRadius, ring, points);
		for(std::vector<Vec2i>::const_iterator i = points.begin(); i != points.end(); ++i)
		{
			if(p_root->getElementAt(*i) == p_element) return true;
		}

		if(ringRadius >= radius) return false;
	}
}

Vec2i GUI::pointFor(GUI_Element* p_element) const
{
	return p_element == p_fingerElement ? cursorPos + fingerOffset : cursorPos;
}

void GUI::renderFrame(const Vec2i& targetPosition,
					  const Vec2i& size,
					  const Vec2i& positionOnTexture,
					  const Vec4f& color)
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
	renderer.quads(renderer.state(), &quads[0], static_cast<uint>(quads.size()), color);
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