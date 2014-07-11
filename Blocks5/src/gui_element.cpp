#include "pch.h"
#include "gui_all.h"
#include "filesystem.h"

uint GUI_Element::numElementsRendered = 0;

GUI_Element::GUI_Element(const std::string& name,
						 GUI_Element* p_parent,
						 const Vec2i& position,
						 const Vec2i& size)
{
	this->name = name;
	if(!p_parent) p_parent = GUI::inst().getRoot();
	this->p_parent = p_parent;
	if(p_parent) p_parent->children.push_back(this);
	this->position = position;
	this->size = size;
	visible = true;
	active = true;
	fill = false;
	fillColor = Vec4d(0.0, 0.0, 0.0, 1.0);
	toolTipOnly = false;
	tabStop = -1;
	p_font = GUI::inst().getFont();
	if(p_font) p_font->addRef();
}

GUI_Element::~GUI_Element()
{
	if(p_parent) p_parent->children.remove(this);
	while(!children.empty()) delete children.front();

	GUI& gui = GUI::inst();
	if(gui.p_elementAtCursor == this) gui.p_elementAtCursor = 0;
	if(gui.p_oldElementAtCursor == this) gui.p_oldElementAtCursor = 0;
	hide();

	if(p_font) p_font->release();
}

void GUI_Element::render()
{
	if(!visible) return;

	glPushMatrix();
	glTranslated(position.x, position.y, 0.0);

	// sich selbst rendern
	onRender();

	// Kinder rendern
	for(std::list<GUI_Element*>::const_iterator i = children.begin(); i != children.end(); ++i)
	{
		(*i)->render();
	}

	onRenderEnd();

	glPopMatrix();

	if(getType() != "GUI_Element") numElementsRendered++;
}

void GUI_Element::update()
{
	// sich selbst aktualisieren
	onUpdate();

	// Kinder aktualisieren
	for(std::list<GUI_Element*>::const_iterator i = children.begin(); i != children.end(); ++i)
	{
		(*i)->update();
	}
}

void GUI_Element::onRender()
{
	if(fill)
	{
		glBegin(GL_QUADS);
		glColor4dv(fillColor);
		glVertex2i(0, 0);
		glVertex2i(size.x, 0);
		glVertex2i(size.x, size.y);
		glVertex2i(0, size.y);
		glEnd();
	}
}

void GUI_Element::onRenderEnd()
{
}

void GUI_Element::onUpdate()
{
}

void GUI_Element::onMouseDown(const Vec2i& position,
							  int buttons)
{
	if(toolTipOnly) p_parent->onMouseDown(this->position + position, buttons);
}

void GUI_Element::onMouseUp(const Vec2i& position,
							int buttons)
{
	if(toolTipOnly) p_parent->onMouseUp(this->position + position, buttons);
}

void GUI_Element::onMouseEnter(int buttons)
{
	if(toolTipOnly) p_parent->onMouseEnter(buttons);
}

void GUI_Element::onMouseLeave(int buttons)
{
	if(toolTipOnly) p_parent->onMouseLeave(buttons);
}

void GUI_Element::onMouseMove(const Vec2i& position,
							  const Vec2i& movement,
							  int buttons)
{
	if(toolTipOnly) p_parent->onMouseMove(this->position + position, movement, buttons);
}

void GUI_Element::onMouseWheel(int dir)
{
	if(toolTipOnly) p_parent->onMouseWheel(dir);
}

void GUI_Element::onKeyEvent(const SDL_KeyboardEvent& event)
{
	if(toolTipOnly)
	{
		p_parent->onKeyEvent(event);
		return;
	}

	// Uns interessiert nur, ob eine Taste gedrückt wurde.
	if(event.type != SDL_KEYDOWN) return;

	// Shift gedrückt?
	bool shift = (event.keysym.mod & KMOD_LSHIFT) || (event.keysym.mod & KMOD_RSHIFT);

	switch(event.keysym.sym)
	{
	case SDLK_TAB:
		{
			GUI_Element* p_focusElement = GUI::inst().getFocusElement();
			if(p_focusElement)
			{
				if(shift)
				{
					GUI_Element* p_element = p_focusElement->getPreviousTabElement();
					p_element->focus();
					p_element->onTabbedIn();
				}
				else
				{
					GUI_Element* p_element = p_focusElement->getNextTabElement();
					p_element->focus();
					p_element->onTabbedIn();
				}
			}
		}
		break;
	default:
		// Ereignis an das Elternelement weiterleiten
		if(p_parent) p_parent->onKeyEvent(event);
		break;
	}
}

void GUI_Element::onTabbedIn()
{
}

std::string GUI_Element::getType() const
{
	return "GUI_Element";
}

GUI_Element* GUI_Element::getElementAt(const Vec2i& position)
{
	if(!visible) return 0;

	// Ist die Position außerhalb der eigenen Grenzen?
	if(position.x < 0 || position.y < 0 || position.x >= size.x || position.y >= size.y) return 0;

	for(std::list<GUI_Element*>::reverse_iterator i = children.rbegin(); i != children.rend(); ++i)
	{
		GUI_Element* p_element = (*i)->getElementAt(position - (*i)->getPosition());
		if(p_element) return p_element;
	}

	// Kein Kind getroffen? Dann sind wir es selbst.
	return this;
}

void GUI_Element::bringToFront()
{
	show();

	if(p_parent)
	{
		// das Element an das Ende der Kinderliste des Elternelements setzen
		p_parent->children.remove(this);
		p_parent->children.push_back(this);

		// das Elternelement ebenfalls in den Vordergrund holen
		p_parent->bringToFront();
	}
}

bool GUI_Element::isFocused()
{
	return this == GUI::inst().getFocusElement();
}

bool GUI_Element::isFocusedIndirectly()
{
	GUI_Element* p_element = GUI::inst().getFocusElement();
	if(!p_element) return false;
	while(p_element)
	{
		if(this == p_element) return true;
		p_element = p_element->getParent();
	}

	return false;
}

void GUI_Element::focus()
{
	bringToFront();
	GUI::inst().setFocusElement(this);
}

void GUI_Element::center(bool h,
						 bool v)
{
	if(!h && !v) return;

	const Vec2i p = getPosition();
	const Vec2i np = (p_parent->getSize() - getSize()) / 2;
	setPosition(Vec2i(h ? np.x : p.x, v ? np.y : p.y));
}

bool GUI_Element::load(const std::string& filename)
{
	// XML-Dokument laden
	std::string text = FileSystem::inst().readStringFromFile(filename);
	TiXmlDocument doc;
	doc.Parse(text.c_str());
	if(doc.ErrorId())
	{
		printfLog("+ ERROR: Could not parse dialog XML file \"%s\" (Error: %d).\n",
				  filename.c_str(),
				  doc.ErrorId());
		return false;
	}

	TiXmlHandle docHandle(&doc);
	TiXmlHandle dialogHandle = docHandle.FirstChildElement("Dialog");
	TiXmlElement* p_dialogElement = dialogHandle.Element();

	return load(p_dialogElement);
}

bool GUI_Element::load(TiXmlElement* p_element)
{
	// Attribute lesen
	readAttributes(p_element);

	// alle Kind-Elemente verarbeiten
	p_element = p_element->FirstChildElement();
	while(p_element)
	{
		// Typ lesen
		const std::string& type = p_element->ValueStr();
		if(type == "Center") center();
		else if(type == "HCenter") center(true, false);
		else if(type == "VCenter") center(false, true);
		else if(type == "Hidden") hide();
		else if(type == "Deactivated") deactivate();
		else if(type == "ToolTip")
		{
			const char* p_toolTip = p_element->GetText();
			toolTip = p_toolTip ? p_toolTip : "";
		}
		else if(type == "ToolTipOnly") setToolTipOnly(true);
		else if(type == "TabStop")
		{
			tabStop = -1;
			p_element->QueryIntAttribute("id", &tabStop);
		}
		else if(type == "Font")
		{
			const char* p_fontName = p_element->GetText();
			if(p_fontName)
			{
				p_font->release();
				p_font = Manager<Font>::inst().request(p_fontName);
			}
		}
		else
		{
			// Name, Position und Größe lesen
			if(p_element->Attribute("name"))
			{
				std::string name = p_element->Attribute("name");
				Vec2i position(0, 0), size(0, 0);
				p_element->QueryIntAttribute("x", &position.x);
				p_element->QueryIntAttribute("y", &position.y);
				p_element->QueryIntAttribute("w", &size.x);
				p_element->QueryIntAttribute("h", &size.y);

				// Element erzeugen
				GUI_Element* p_child = 0;
				if(type == "Button") p_child = new GUI_Button(name, this, position, size);
				else if(type == "CheckBox") p_child = new GUI_CheckBox(name, this, position, size);
				else if(type == "EditBox") p_child = new GUI_EditBox(name, this, position, size);
				else if(type == "Element") p_child = new GUI_Element(name, this, position, size);
				else if(type == "ListBox") p_child = new GUI_ListBox(name, this, position, size);
				else if(type == "MultiLineEditBox") p_child = new GUI_MultiLineEditBox(name, this, position, size);
				else if(type == "RadioButton") p_child = new GUI_RadioButton(name, this, position, size);
				else if(type == "ScrollBar") p_child = new GUI_ScrollBar(name, this, position, size);
				else if(type == "StaticImage") p_child = new GUI_StaticImage(name, this, position, size);
				else if(type == "StaticText") p_child = new GUI_StaticText(name, this, position, size);
				else if(type == "Window") p_child = new GUI_Window(name, this, position, size);

				if(p_child)
				{
					// Rekursion
					p_child->load(p_element);
				}
			}
		}

		p_element = p_element->NextSiblingElement();
	}

	return true;
}

void GUI_Element::readAttributes(TiXmlElement* p_element)
{
	TiXmlElement* e = p_element->FirstChildElement("Fill");
	if(e)
	{
		fill = true;
		e->Attribute("r", &fillColor.r);
		e->Attribute("g", &fillColor.g);
		e->Attribute("b", &fillColor.b);
		e->Attribute("a", &fillColor.a);
	}
}

GUI_Element* GUI_Element::getChild(const std::string& name)
{
	return GUI::inst()[getFullName() + "." + name];
}

GUI_Element* GUI_Element::operator [] (const std::string& name)
{
	return getChild(name);
}

bool GUI_Element::isChildOf(GUI_Element* p_element) const
{
	GUI_Element* e = this->p_parent;
	while(e)
	{
		if(e == p_element) return true;
		e = e->p_parent;
	}

	return false;
}

GUI_Element* GUI_Element::getNextTabElement()
{
	if(!p_parent || tabStop == -1) return this;

	// Geschwisterelement mit der nächst höheren Tabstop-ID suchen
	int minID = 0x7FFFFFFF;
	GUI_Element* p_minElement = 0;
	const std::list<GUI_Element*>& siblings = p_parent->getChildren();
	for(std::list<GUI_Element*>::const_iterator i = siblings.begin(); i != siblings.end(); ++i)
	{
		const GUI_Element* p_element = *i;
		if(p_element != this &&
		   p_element->isActive() &&
		   p_element->isVisible() &&
		   p_element->getTabStop() != -1 &&
		   p_element->getTabStop() > tabStop &&
		   p_element->getTabStop() < minID)
		{
			minID = p_element->getTabStop();
			p_minElement = const_cast<GUI_Element*>(p_element);
		}
	}

	if(p_minElement) return p_minElement;

	// Es gibt nichts passendes. Das Geschwisterelement mit der niedrigsten Tabstop-ID überhaupt suchen.
	for(std::list<GUI_Element*>::const_iterator i = siblings.begin(); i != siblings.end(); ++i)
	{
		const GUI_Element* p_element = *i;
		if(p_element != this &&
		   p_element->isActive() &&
		   p_element->isVisible() &&
		   p_element->getTabStop() != -1 &&
		   p_element->getTabStop() < minID)
		{
			minID = p_element->getTabStop();
			p_minElement = const_cast<GUI_Element*>(p_element);
		}
	}

	if(p_minElement) return p_minElement;
	else return this;
}

GUI_Element* GUI_Element::getPreviousTabElement()
{
	if(!p_parent || tabStop == -1) return this;

	// Geschwisterelement mit der nächst niedrigeren Tabstop-ID suchen
	int maxID = -1;
	GUI_Element* p_maxElement = 0;
	const std::list<GUI_Element*>& siblings = p_parent->getChildren();
	for(std::list<GUI_Element*>::const_iterator i = siblings.begin(); i != siblings.end(); ++i)
	{
		const GUI_Element* p_element = *i;
		if(p_element != this &&
		   p_element->isActive() &&
		   p_element->isVisible() &&
		   p_element->getTabStop() != -1 &&
		   p_element->getTabStop() < tabStop &&
		   p_element->getTabStop() > maxID)
		{
			maxID = p_element->getTabStop();
			p_maxElement = const_cast<GUI_Element*>(p_element);
		}
	}

	if(p_maxElement) return p_maxElement;

	// Es gibt nichts passendes. Das Geschwisterelement mit der höchsten Tabstop-ID überhaupt suchen.
	for(std::list<GUI_Element*>::const_iterator i = siblings.begin(); i != siblings.end(); ++i)
	{
		const GUI_Element* p_element = *i;
		if(p_element != this &&
		   p_element->isActive() &&
		   p_element->isVisible() &&
		   p_element->getTabStop() != -1 &&
		   p_element->getTabStop() > maxID)
		{
			maxID = p_element->getTabStop();
			p_maxElement = const_cast<GUI_Element*>(p_element);
		}
	}

	if(p_maxElement) return p_maxElement;
	else return this;
}

const std::string& GUI_Element::getName() const
{
	return name;
}

std::string GUI_Element::getFullName() const
{
	std::string fullName = name;
	GUI_Element* p_element = p_parent;
	while(p_element != GUI::inst().getRoot())
	{
		fullName = p_element->getName() + "." + fullName;
		p_element = p_element->p_parent;
	}

	return fullName;
}

GUI_Element* GUI_Element::getParent()
{
	return p_parent;
}

const std::list<GUI_Element*>& GUI_Element::getChildren() const
{
	return children;
}

const Vec2i& GUI_Element::getPosition() const
{
	return position;
}

Vec2i GUI_Element::getAbsPosition() const
{
	if(!p_parent) return position;
	else return position + p_parent->getAbsPosition();
}

void GUI_Element::setPosition(const Vec2i& position)
{
	this->position = position;
}

void GUI_Element::setAbsPosition(const Vec2i& absPosition)
{
	const Vec2i myAbsPosition = getAbsPosition();
	setPosition(absPosition - myAbsPosition);
}

const Vec2i& GUI_Element::getSize() const
{
	return size;
}

void GUI_Element::setSize(const Vec2i& size)
{
	this->size = size;
}

bool GUI_Element::isVisible() const
{
	return visible;
}

bool GUI_Element::isReallyVisible() const
{
	if(!visible) return false;
	else if(p_parent) return p_parent->isReallyVisible();
	else return true;
}

void GUI_Element::show()
{
	visible = true;
}

void GUI_Element::hide()
{
	visible = false;
	if(isFocusedIndirectly()) GUI::inst().setFocusElement(0);
}

bool GUI_Element::isActive() const
{
	return active;
}

void GUI_Element::activate()
{
	active = true;
}

void GUI_Element::deactivate()
{
	active = false;
}

const std::string& GUI_Element::getToolTip() const
{
	return toolTip;
}

void GUI_Element::setToolTip(const std::string& toolTip)
{
	this->toolTip = toolTip;
}

bool GUI_Element::isToolTipOnly() const
{
	return toolTipOnly;
}

void GUI_Element::setToolTipOnly(bool toolTipOnly)
{
	this->toolTipOnly = toolTipOnly;
}

int GUI_Element::getTabStop() const
{
	return tabStop;
}

void GUI_Element::setTabStop(int tabStop)
{
	this->tabStop = tabStop;
}

bool GUI_Element::useSkin() const
{
	return GUI::inst().getSkin() != 0;
}