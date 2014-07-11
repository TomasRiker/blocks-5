#ifndef _GUI_H
#define _GUI_H

/*** Klasse für die Benutzeroberfläche ***/

class GUI_Element;
class Font;
class Texture;

class GUI : public Singleton<GUI>
{
	friend class Singleton<GUI>;
	friend class GUI_Element;

public:
	bool init();
	void exit();
	void render();
	void renderToolTip();
	void display();
	void update();

	void renderFrame(const Vec2i& targetPosition, const Vec2i& size, const Vec2i& positionOnTexture);

	GUI_Element* getElement(const std::string& fullName);
	GUI_Element* operator [] (const std::string& fullName);

	GUI_Element* getRoot();
	Font* getFont();
	Font* getToolTipFont();
	double getOpacity() const;
	void setOpacity(double opacity);
	Texture* getSkin();
	void setSkin(Texture* p_skin);
	const Vec2i& getCursorPos() const;
	GUI_Element* getFocusElement();
	void setFocusElement(GUI_Element* p_element);
	GUI_Element* getOldFocusElement();
	GUI_Element* getMouseDownElement();
	const std::string& getClipboard() const;
	void setClipboard(const std::string& clipboard);

private:
	GUI();
	~GUI();

	bool initialized;
	GUI_Element* p_root;
	Font* p_font;
	Font* p_toolTipFont;
	uint texID;
	double opacity;
	Texture* p_skin;

	Vec2i cursorPos;
	Vec2i oldCursorPos;
	GUI_Element* p_elementAtCursor;
	GUI_Element* p_oldElementAtCursor;
	GUI_Element* p_focusElement;
	GUI_Element* p_oldFocusElement;
	GUI_Element* p_mouseDownElement;
	std::string clipboard;
	uint noMoveCounter;
};

#endif