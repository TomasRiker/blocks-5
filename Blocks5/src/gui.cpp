#include "pch.h"
#include "gui.h"
#include "gui_element.h"
#include "font.h"
#include "texture.h"
#include "engine.h"
#include "touchkeyboard.h"

namespace
{
	// How far a finger reaches, and how much nearer than any other the
	// element it meant has to be, in reference pixels
	// (Engine::getReferencePixelScale): a physical length, about the same on
	// a phone and on a Windows tablet. 16 lifts an 18-pixel button on a phone
	// in landscape to 47 CSS pixels, past the 44 the accessibility guidance
	// asks for a finger. Both are feel, for the author to tune on a device: a
	// larger reach catches wider misses, a larger margin leaves more taps
	// between two targets to nobody.
	const float TOUCH_RADIUS = 16.0f;
	const float TOUCH_MARGIN = 4.0f;

	// How far a finger on an element that pans may move from where it pressed
	// and still tap, in reference pixels - Android's touch slop. Further, it
	// pans the element (GUI_Element::pansAt).
	const float TOUCH_SLOP = 8.0f;

	// An element let go of while it pans glides on and slows down, keeping
	// this share of its speed a second; iOS's scroll views keep 0.135. Let go
	// of slower than GLIDE_START, in reference pixels a second, it stays where
	// it is, and below GLIDE_STOP it stops. All three are feel, for the author
	// to tune on a device: a larger GLIDE_KEEP glides further and longer, a
	// larger GLIDE_START leaves more slow releases where they are.
	const float GLIDE_KEEP = 0.1f;
	const float GLIDE_START = 50.0f;
	const float GLIDE_STOP = 10.0f;

	// A finger that held still this long before it lifted, in milliseconds by
	// the clock the ticks stand for (Engine::getInputTime), meant to leave the
	// element where it is, however fast it moved before: Android's 40, and
	// room for a frame of 50, since Windows and the browser read the finger
	// once a frame. Not counted in ticks: of a frame that runs three or more -
	// under twenty frames a second - one tick would see the finger move and
	// the others not, and every flick would read as a finger held still.
	const Uint32 STILL_TIME = 60;

	// How much of the finger's way its speed at the lift is measured over, in
	// milliseconds by the same clock - at least the last step, however long
	// that took. Not over ticks either: where the finger is read once a frame,
	// a frame that took long - a hitch - hands the end of a slow drag to one
	// tick, and its step would read as a flick. And only the ticks it moved in
	// count: the ticks of such a frame before the one that sees the move read
	// the finger where it was, and counting them would put the whole step
	// inside the last SPEED_TIME.
	const Uint32 SPEED_TIME = 100;

	// The reach in game pixels, bounded so that a canvas squeezed to the size
	// of a stamp cannot ask for a search the size of the screen.
	float touchReach()
	{
		return min(TOUCH_RADIUS * Engine::inst().getReferencePixelScale(), 64.0f);
	}

	// The slop in game pixels, bounded as the reach is, so that a finger that
	// pressed on an element itself cannot leave its reach while it may still
	// tap. One moved onto it from a near miss can, and its tap then lets go.
	float touchSlop()
	{
		return min(TOUCH_SLOP * Engine::inst().getReferencePixelScale(), touchReach());
	}

	// The text field an element is or belongs to: a multi-line box's own
	// scroll bars hand its keys on to it, and it still draws as focused.
	GUI_Element* textFieldOf(GUI_Element* p_element)
	{
		for(; p_element; p_element = p_element->getParent())
		{
			if(p_element->takesText()) return p_element;
		}
		return 0;
	}

	// Whether a press at this point, in screen coordinates, does something
	// where it lands.
	bool takesPress(GUI_Element* p_element, const Vec2i& point)
	{
		return p_element->isClickTarget(point - p_element->getAbsPosition());
	}

	struct TouchOffset
	{
		Vec2i offset;
		float distance;
	};

	bool nearerFirst(const TouchOffset& a, const TouchOffset& b)
	{
		if(a.distance != b.distance) return a.distance < b.distance;
		if(a.offset.y != b.offset.y) return a.offset.y < b.offset.y;
		return a.offset.x < b.offset.x;
	}

	// Every whole pixel within the reach but the point itself, nearest first:
	// the order the search asks getElementAt in, so the first pixel of a
	// target it meets is that target's nearest, and distances, ties and
	// landings are exact rather than as fine as some sampling. Built again
	// only when the reach changes.
	const std::vector<TouchOffset>& offsetsWithin(float radius)
	{
		static std::vector<TouchOffset> offsets;
		static float builtFor = -1.0f;
		if(radius != builtFor)
		{
			offsets.clear();
			const int r = static_cast<int>(floorf(radius));
			for(int dy = -r; dy <= r; dy++)
			{
				for(int dx = -r; dx <= r; dx++)
				{
					const float distance = sqrtf(static_cast<float>(dx * dx + dy * dy));
					if((dx || dy) && distance <= radius)
					{
						TouchOffset o = { Vec2i(dx, dy), distance };
						offsets.push_back(o);
					}
				}
			}
			std::sort(offsets.begin(), offsets.end(), nearerFirst);
			builtFor = radius;
		}
		return offsets;
	}

	// renderBackdrop's patch in gui.png: the cell at (144, 96) less a texel
	// all round, which stays transparent because linear filtering reaches one
	// texel past the patch's edge. White, its alpha a ramp across times a ramp
	// down, which is what a blurred rectangle is and rounds its corners off,
	// each ramp a smoothstep over BACKDROP_RAMP texels. The opaque core
	// between them is what a patch stretches, so its edge is as soft whatever
	// its size.
	const Vec2i BACKDROP_ON_SKIN(145, 97);
	const int BACKDROP_RAMP = 22;
	const int BACKDROP_CORE = 2;
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
	mouseDownButtons = 0;
	buttonsDownBefore = 0;
	cursorOnPicture = true;
	p_fingerElement = 0;
	fingerOffset = Vec2i(0, 0);
	fingerHolds = false;
	fingerOff = false;
	p_panElement = 0;
	panning = panCaught = false;
	panSlop = 0.0f;
	panMovedAt = 0;
	panTrailLength = 0;
	p_glideElement = 0;
	touchKeyboardWanted = false;
	glideSpeed = glideRest = Vec2f(0.0f, 0.0f);
	glideKeep = glideStop = 0.0f;
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
	cursorOnPicture = Engine::inst().isCursorOnPicture();
	oldRawCursorPos = Engine::inst().getRawCursorPosition();
	p_elementAtCursor = p_oldElementAtCursor = p_root;
	p_focusElement = 0;
	p_oldFocusElement = 0;
	p_mouseDownElement = 0;
	mouseDownButtons = 0;
	buttonsDownBefore = 0;
	p_fingerElement = 0;
	fingerOffset = Vec2i(0, 0);
	fingerHolds = false;
	fingerOff = false;
	p_panElement = 0;
	p_glideElement = 0;
	touchKeyboardWanted = false;
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
	int buttonsReleased = (engine.wasButtonReleased(1) ? 1 : 0) | (engine.wasButtonReleased(3) ? 2 : 0);
	const int downBefore = buttonsDownBefore;
	buttonsDownBefore = buttonsDown;

	// A release and a press in one tick with the button down before them: the
	// release came first - SDL posts every edge - and ends the gesture before,
	// at the point it was last followed to: a finger's pan, a tap or a glide
	// the new press may catch below; or a press handed over, told of its
	// release where the cursor stood, which this tick has not moved yet. What
	// held that gesture is asked first, as every tick asks it below: hidden,
	// covered or out of reach since - a pane a key opened after the last
	// update() - it lets go without its click or its tap. Down again at the
	// end, the release is spent on that gesture: handed on, it would end the
	// new one in the tick it began, tapping a list that was to be dragged and
	// clicking a button on its press. Up at the end, the new press was let go
	// of in this tick too - two quick taps in one frame - and the release is
	// its tap's as well.
	if((downBefore & 1) && (buttonsPressed & 1) && (buttonsReleased & 1))
	{
		if(fingerHolds && (!p_fingerElement->isReallyVisible() || !cursorOnPicture || !fingerReaches(p_fingerElement, cursorPos)))
			fingerHolds = false;
		GUI_Element* p_under = fingerOff ? 0 : fingerHolds ? p_fingerElement : p_root->getElementAt(cursorPos);
		if(p_under != p_elementAtCursor)
		{
			if(p_elementAtCursor) p_elementAtCursor->onMouseLeave(downBefore);
			if(p_under) p_under->onMouseEnter(downBefore);
			p_elementAtCursor = p_under;
		}

		if(p_panElement)
		{
			if(p_panElement->isReallyVisible() && (panning || fingerHolds)) releasePan(panPoint);
			p_panElement = 0;
		}
		else
		{
			if(p_elementAtCursor && p_elementAtCursor->isReallyVisible())
				p_elementAtCursor->onMouseUp(pointFor(p_elementAtCursor) - p_elementAtCursor->getAbsPosition(), 1);
			if(p_mouseDownElement && p_mouseDownElement != p_elementAtCursor)
				p_mouseDownElement->onMouseUp(pointFor(p_mouseDownElement) - p_mouseDownElement->getAbsPosition(), 1);
		}
		p_mouseDownElement = 0;
		if(buttonsDown & 1) buttonsReleased &= ~1;
	}
	// Down before and up now with no release, or pressed again with none
	// between: the release never came - the window lost the focus, after
	// which SDL 1.2 posts none under Windows, or the browser cancelled the
	// touch (Engine's CANCELLED_TOUCH) - and the gesture it was to end is
	// dropped before anything moves. Held on to, every later move would drag
	// a slider pressed before, a button would fire on some later release, and
	// a pan would jump to a new press and follow it.
	else if(((downBefore & 1) && (buttonsPressed & 1)) ||
			((downBefore & ~buttonsDown & ~buttonsReleased) && (p_mouseDownElement || p_panElement)))
		dropGesture();
	// A press, its release and a press again with nothing down before: one
	// tick hands over one press, the last, and the release goes with the
	// first - told to what the cursor stood on before, it would end a
	// gesture that never began there.
	else if((buttonsPressed & 1) && (buttonsReleased & 1) && (buttonsDown & 1)) buttonsReleased &= ~1;

	// A glide goes on by itself until a press anywhere, a key or the element
	// leaving the screen stops it. A finger's press on the gliding element
	// itself only catches it, so it taps nothing when it lifts (releasePan).
	GUI_Element* p_caught = 0;
	if(p_glideElement)
	{
		if(buttonsPressed)
		{
			p_caught = p_glideElement;
			p_glideElement = 0;
		}
		else if(!p_glideElement->isReallyVisible()) p_glideElement = 0;
		else glide();
	}

	// update the mouse position and the element under it
	oldCursorPos = cursorPos;
	cursorPos = engine.getCursorPosition();
	cursorOnPicture = engine.isCursorOnPicture();
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
	// cancel its own press. Out of reach - off the picture is out of reach -
	// hidden or covered, it lets go, as a mouse dragged off a button does -
	// asked every tick and not only when the finger moves, since a still
	// finger is how a pane opening over the element would otherwise go
	// unnoticed. The engine's cursor stays where the finger is.
	//
	// A finger's press outside the picture - in a black bar beside it, where
	// a browser's canvas goes on and the pad's buttons stand - is on nothing
	// of the game's until it lifts: cursorPos is the picture's edge there,
	// and the reach would find whatever stands near the edge, however far
	// off the finger is. A mouse there is on the edge, as it always was.
	if(buttonsPressed)
	{
		fingerOff = engine.wasFingerPress() && !cursorOnPicture;
		Vec2i landing = cursorPos;
		p_fingerElement = engine.wasFingerPress() && !fingerOff ? pickTouchTarget(cursorPos, &landing) : 0;
		fingerOffset = landing - cursorPos;
		fingerHolds = p_fingerElement != 0;
	}
	else if(fingerHolds && (!p_fingerElement->isReallyVisible() || !cursorOnPicture || !fingerReaches(p_fingerElement, cursorPos)))
	{
		fingerHolds = false;
	}

	p_elementAtCursor = fingerOff ? 0 : fingerHolds ? p_fingerElement : p_root->getElementAt(cursorPos);

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

			// A finger on an element that pans may be tapping it or starting
			// to drag it, which only its moves can tell, so the element does
			// not hear of the press yet (followPan, releasePan): a list that
			// selected on the press would select an item with every scroll,
			// and set off whatever its selection does.
			if(engine.wasFingerPress() && buttonsPressed == 1 && p_elementAtCursor->pansAt(relCursorPos))
			{
				p_panElement = p_elementAtCursor;
				panning = false;
				panCaught = p_elementAtCursor == p_caught;
				panPress = relCursorPos;
				panStart = panPoint = pointFor(p_elementAtCursor);
				panSlop = touchSlop();
				panTrailLength = 0;
				p_mouseDownElement = 0;
			}
			else
			{
				p_elementAtCursor->onMouseDown(relCursorPos, buttonsPressed);
				/* if(buttonsPressed & 1) */ p_mouseDownElement = p_elementAtCursor;
				mouseDownButtons = buttonsPressed;
				if((engine.wasFingerPress() || engine.wasPenPress()) && (buttonsPressed & 1)) fingerFocused(p_elementAtCursor);
			}
		}

		if(buttonsDown & 1) noMoveCounter = 10;

		if(buttonsReleased)
		{
			// The element a finger pans hears of its release from releasePan.
			// An element an earlier call took away is 0 by now.
			if(p_elementAtCursor && p_elementAtCursor != p_panElement) p_elementAtCursor->onMouseUp(relCursorPos, buttonsReleased);

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
			// What a press's own tick reports moved is the way the pointer
			// came, not a drag: a finger lands with no move before it, so its
			// press arrives together with the jump from wherever the last one
			// lifted, and a window pressed on its title bar would follow it.
			const Vec2i movement = buttonsPressed ? Vec2i(0, 0) : cursorMovement;

			// tell the element about it - not one a finger pans, which
			// follows it through onPan
			if(p_elementAtCursor && p_elementAtCursor != p_panElement) p_elementAtCursor->onMouseMove(relCursorPos, movement, buttonsDown);

			if(p_mouseDownElement &&
			   p_mouseDownElement != p_elementAtCursor)
			{
				// inform the element under the cursor at the time of the press
				p_mouseDownElement->onMouseMove(pointFor(p_mouseDownElement) - p_mouseDownElement->getAbsPosition(), movement, buttonsDown);
			}
		}
	}

	// The finger on an element that pans, wherever the finger is now: the
	// element keeps the gesture, as a mouse's press keeps its element when
	// the mouse leaves it. A tap whose press and release came in one tick is
	// handed over here in that tick. Up without a release in this tick, the
	// release went by unseen - the window lost the focus - and the gesture
	// with it; held on to, every later move of the mouse would pan.
	if(p_panElement)
	{
		// Still a tap, it lets go once the finger no longer holds the
		// element - covered by a pane, or out of reach - as a held button
		// does: a list would otherwise select behind the pane, and a second
		// tap click its submit button there.
		if(!p_panElement->isReallyVisible() || !((buttonsDown & 1) || (buttonsReleased & 1)) || (!panning && !fingerHolds))
			p_panElement = 0;
		else
		{
			const Vec2i point = pointFor(p_panElement);
			followPan(point);
			if(p_panElement && (buttonsReleased & 1)) releasePan(point);
		}
	}

	// Every button up and a press still held: a press whose release this
	// tick took away again - a touch the browser cancelled as it began - or
	// one its element did not hear of, gone from the screen. Let go of the
	// same way.
	if(!buttonsDown && (p_mouseDownElement || p_panElement)) dropGesture();

	// The touch keyboard goes once the focus has left the text fields - a
	// button pressed, a dialog closed - as Windows takes it from its own.
	GUI_Element* p_textField = textFieldOf(p_focusElement);
	if(touchKeyboardWanted && !(p_textField && p_textField->isReallyVisible()))
	{
		touchKeyboardWanted = false;
		TouchKeyboard::hide();
	}

	// Off the glass, the gesture is over: from the next tick on, what lies
	// under the cursor is under it again. A tap whose press and release came
	// in one tick got both above.
	if(!buttonsDown)
	{
		p_fingerElement = 0;
		fingerHolds = false;
		fingerOff = false;
	}

	// Keyboard events?
	SDL_KeyboardEvent event;
	while(engine.getKeyEvent(&event, &keyRepeat))
	{
		if(event.type == SDL_KEYDOWN) p_glideElement = 0;
		if(p_focusElement) p_focusElement->onKeyEvent(event);
	}
	keyRepeat = false;

	// Mouse wheel?
	if(p_elementAtCursor)
	{
		int wheel = 0;
		if(engine.wasButtonPressed(SDL_BUTTON_WHEELUP)) wheel = -1;
		else if(engine.wasButtonPressed(SDL_BUTTON_WHEELDOWN)) wheel = 1;
		if(wheel)
		{
			p_glideElement = 0;
			p_elementAtCursor->onMouseWheel(wheel);
		}
	}

	if(!cursorMoved) noMoveCounter = min<uint>(20, noMoveCounter + 1);
	else if(p_elementAtCursor != p_oldElementAtCursor && noMoveCounter) noMoveCounter--;

	if(p_elementAtCursor && p_elementAtCursor->getToolTip().empty() && noMoveCounter) --noMoveCounter;
}

// A finger is not a point. Searched outward from the point pixel by pixel,
// nearest first, asking at every pixel what a press there would hit -
// getElementAt(), so that z-order, a pane in front, a modal one over
// everything, a hidden element and every overridden hit shape come out
// exactly as for a mouse. Of what takes a press there, the nearest wins: by
// distance, not by how much of the disc it covers, which a large list would
// win against a small button every time. A label and the box it labels are
// one target, so a finger between the two is not torn between them.
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

	const float margin = TOUCH_MARGIN * Engine::inst().getReferencePixelScale();
	const std::vector<TouchOffset>& offsets = offsetsWithin(touchReach());

	// The nearest target and where, and whether a second one was met within
	// the margin of it. A greyed-out target counts as near as it is: it
	// cannot win, but the finger that meant it gets the dead tap a press on
	// it would, not the button beside it.
	GUI_Element* p_receiver = 0;
	GUI_Element* p_element = 0;
	float distance = 0.0f;
	bool enabled = false;
	bool rival = false;
	for(std::vector<TouchOffset>::const_iterator i = offsets.begin(); i != offsets.end(); ++i)
	{
		// From here on everything is further than the margin beyond the
		// nearest: no rival to it.
		if(p_receiver && i->distance >= distance + margin) break;

		const Vec2i at = point + i->offset;
		GUI_Element* p_hit = p_root->getElementAt(at);
		if(!p_hit || !takesPress(p_hit, at)) continue;

		GUI_Element* p_hitReceiver = p_hit->getPressReceiver();
		if(!p_hitReceiver->isReallyVisible()) continue;

		if(!p_receiver)
		{
			p_receiver = p_hitReceiver;
			p_element = p_hit;
			distance = i->distance;
			enabled = p_hit->isActive() && p_hitReceiver->isActive();
			*p_landing = at;
		}
		else if(p_hitReceiver != p_receiver)
		{
			rival = true;
			break;
		}
	}

	if(!p_receiver || !enabled || rival)
	{
		*p_landing = point;
		return p_exact;
	}
	return p_element;
}

bool GUI::fingerReaches(GUI_Element* p_element, const Vec2i& point)
{
	if(!p_root) return false;
	if(p_root->getElementAt(point) == p_element) return true;

	const std::vector<TouchOffset>& offsets = offsetsWithin(touchReach());
	for(std::vector<TouchOffset>::const_iterator i = offsets.begin(); i != offsets.end(); ++i)
	{
		if(p_root->getElementAt(point + i->offset) == p_element) return true;
	}
	return false;
}

GUI_Element* GUI::textFieldTapped(const Vec2i& press,
								  const Vec2i& farthest)
{
	if(!p_root) return 0;

	// A tap and not the start of a drag: the finger never went further from
	// where it pressed than the slop that tells the two apart in a pan.
	if(static_cast<Vec2f>(farthest - press).length() > touchSlop()) return 0;

	GUI_Element* p_pressed;
	if(p_fingerElement)
	{
		// update() has handed the press over, and its record decides, not
		// what lies at the point now: the press may have changed what is
		// there - a note's editor opened by a finger on the level. No tap
		// where the finger has let it go (out of reach, covered), caught a
		// glide with it, or pans.
		if(!fingerHolds) return 0;
		p_pressed = p_fingerElement;
		if(p_pressed == p_panElement && (panning || panCaught)) return 0;
	}
	else
	{
		// Not yet: what the press will go to, which will catch a glide of
		// its own and tap nothing, and lets go where the finger leaves its
		// reach.
		Vec2i landing;
		p_pressed = pickTouchTarget(press, &landing);
		if(!p_pressed || p_pressed == p_glideElement) return 0;
		if(!fingerReaches(p_pressed, farthest)) return 0;
	}

	GUI_Element* p_field = p_pressed->getPressReceiver();
	return p_field->takesText() && p_field->isReallyVisible() ? p_field : 0;
}

bool GUI::isTouchKeyboardWanted() const
{
	return touchKeyboardWanted;
}

void GUI::fingerFocused(GUI_Element* p_pressed)
{
	// Only where the press went to the field, or to its label: a press that
	// makes the game move the focus into one - a note's editor opened by a
	// finger on the level - asks for nothing, as the browser opens no sheet
	// for it. Every tap and not only the first: a keyboard dismissed by hand
	// comes back for a tap on the field it left.
	if(p_focusElement && p_focusElement->takesText() && p_pressed && p_pressed->getPressReceiver() == p_focusElement)
	{
		touchKeyboardWanted = true;
		TouchKeyboard::show();
	}
}

Vec2i GUI::pointFor(GUI_Element* p_element) const
{
	return p_element == p_fingerElement ? cursorPos + fingerOffset : cursorPos;
}

void GUI::followPan(const Vec2i& point)
{
	if(!panning)
	{
		const Vec2f moved = static_cast<Vec2f>(point - panStart);
		const float distance = moved.length();
		if(distance <= panSlop) return;

		// From the edge of the slop and not from the press, or the element
		// would jump by the whole slop the moment it starts to follow.
		panning = true;
		const Vec2f edge = moved * (panSlop / distance);
		panPoint = panStart + Vec2i(static_cast<int>(floorf(edge.x + 0.5f)), static_cast<int>(floorf(edge.y + 0.5f)));
	}

	const Vec2i movement = point - panPoint;
	const bool moved = movement.x || movement.y;
	if(moved)
	{
		panMovedAt = Engine::inst().getInputTime();
		p_panElement->onPan(movement);
	}
	panPoint = point;

	// The trail holds the ticks the finger moved in (SPEED_TIME).
	if(!moved && panTrailLength > 0) return;
	for(int i = PAN_TRAIL - 1; i > 0; i--)
	{
		panTrail[i] = panTrail[i - 1];
		panTrailTime[i] = panTrailTime[i - 1];
	}
	panTrail[0] = point;
	panTrailTime[0] = Engine::inst().getInputTime();
	if(panTrailLength < PAN_TRAIL) panTrailLength++;
}

void GUI::releasePan(const Vec2i& point)
{
	if(!panning)
	{
		// A tap - unless all it did was catch the element gliding: the press
		// where it landed and the release, in one tick. The press can take
		// the element away - a double click that closes its dialog - and the
		// destructor then clears p_panElement.
		if(!panCaught)
		{
			GUI_Element* p_tapped = p_panElement;
			p_panElement->onMouseDown(panPress, 1);
			if(p_panElement) p_panElement->onMouseUp(point - p_panElement->getAbsPosition(), 1);
			if(p_panElement) fingerFocused(p_tapped);
		}
		p_panElement = 0;
		return;
	}

	GUI_Element* p_element = p_panElement;
	p_panElement = 0;

	// The speed it lifted at, in game pixels a tick, over the last SPEED_TIME
	// of its way by the clock or at least its last step - none if it held
	// still before it lifted.
	if(Engine::inst().getInputTime() - panMovedAt >= STILL_TIME) return;
	if(panTrailLength < 2) return;
	int oldest = 1;
	while(oldest + 1 < panTrailLength && panTrailTime[0] - panTrailTime[oldest + 1] <= SPEED_TIME) oldest++;
	const Uint32 elapsed = panTrailTime[0] - panTrailTime[oldest];
	if(!elapsed) return;
	Engine& engine = Engine::inst();
	const int rate = static_cast<int>(engine.getLogicRate());
	const Vec2f speed = static_cast<Vec2f>(panTrail[0] - panTrail[oldest]) * (static_cast<float>(rate) / static_cast<float>(elapsed));
	const float perTick = engine.getReferencePixelScale() * static_cast<float>(rate) / 1000.0f;
	if(speed.length() < GLIDE_START * perTick) return;

	p_glideElement = p_element;
	glideSpeed = speed;
	glideRest = Vec2f(0.0f, 0.0f);
	glideKeep = powf(GLIDE_KEEP, static_cast<float>(rate) / 1000.0f);
	glideStop = GLIDE_STOP * perTick;
}

void GUI::dropGesture()
{
	if(p_elementAtCursor) p_elementAtCursor->onMouseLeave(0);
	if(p_mouseDownElement && p_mouseDownElement != p_elementAtCursor) p_mouseDownElement->onMouseLeave(0);
	// A leave can take an element away, and its destructor clears the pointer.
	if(p_mouseDownElement)
		p_mouseDownElement->onMouseUp(pointFor(p_mouseDownElement) - p_mouseDownElement->getAbsPosition(), mouseDownButtons);
	p_mouseDownElement = 0;
	p_panElement = 0;
	p_fingerElement = 0;
	fingerHolds = false;
	fingerOff = false;
	// Entered again by whatever is under the pointer at the next look.
	p_elementAtCursor = 0;
}

void GUI::glide()
{
	glideRest += glideSpeed;
	const Vec2i step(static_cast<int>(floorf(glideRest.x + 0.5f)), static_cast<int>(floorf(glideRest.y + 0.5f)));
	glideRest -= static_cast<Vec2f>(step);
	glideSpeed *= glideKeep;

	// An element at an end stays put, and the glide is over.
	if((step.x || step.y) && !p_glideElement->onPan(step)) p_glideElement = 0;
	else if(glideSpeed.length() < glideStop) p_glideElement = 0;
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

void GUI::renderBackdrop(const Vec2i& targetPosition,
						 const Vec2i& size,
						 int softness,
						 float alpha)
{
	if(!p_skin) return;

	// Nine pieces: the corners and the edges are the ramps, softness pixels
	// deep, and the middle is the core. A patch narrower than two ramps is
	// all ramp.
	const float soft = static_cast<float>(min(softness, min(size.x, size.y) / 2));
	const Vec2f from = static_cast<Vec2f>(targetPosition);
	const Vec2f to = static_cast<Vec2f>(targetPosition + size);
	const float xs[4] = {from.x, from.x + soft, to.x - soft, to.x};
	const float ys[4] = {from.y, from.y + soft, to.y - soft, to.y};
	const Vec2f origin = static_cast<Vec2f>(BACKDROP_ON_SKIN);
	const float ramp = static_cast<float>(BACKDROP_RAMP);
	const float core = static_cast<float>(BACKDROP_CORE);
	const float us[4] = {origin.x, origin.x + ramp, origin.x + ramp + core, origin.x + 2.0f * ramp + core};
	const float vs[4] = {origin.y, origin.y + ramp, origin.y + ramp + core, origin.y + 2.0f * ramp + core};

	Renderer& renderer = Renderer::inst();
	renderer.setTexture(p_skin->ref());
	const Vec4f color(0.0f, 0.0f, 0.0f, alpha);
	for(int j = 0; j < 3; j++)
	{
		for(int i = 0; i < 3; i++)
		{
			const Vec2f corners[4] = {Vec2f(xs[i], ys[j]), Vec2f(xs[i + 1], ys[j]),
									  Vec2f(xs[i + 1], ys[j + 1]), Vec2f(xs[i], ys[j + 1])};
			const Vec2f uvs[4] = {Vec2f(us[i], vs[j]), Vec2f(us[i + 1], vs[j]),
								  Vec2f(us[i + 1], vs[j + 1]), Vec2f(us[i], vs[j + 1])};
			renderer.quad(renderer.state(), corners, uvs, color);
		}
	}
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