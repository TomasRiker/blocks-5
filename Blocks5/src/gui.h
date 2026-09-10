#ifndef _GUI_H
#define _GUI_H

/*** Class for the user interface ***/

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

	// Is the key just arriving in onKeyEvent() the repeat of a held one?
	// Anything that reads it as a command - Escape, Return, the editors'
	// shortcuts - must skip such a repeat, or a held finger triggers the
	// command again every 60 ms. An edit box and a list, by contrast, want
	// them and do not ask at all.
	bool isKeyRepeat() const;
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

	// Valid only during an onKeyEvent(); isKeyRepeat() reads it.
	bool keyRepeat;

	Vec2i cursorPos;
	Vec2i oldCursorPos;
	// The cursor position as the window reports it - see update().
	Vec2i oldRawCursorPos;
	GUI_Element* p_elementAtCursor;
	GUI_Element* p_oldElementAtCursor;
	GUI_Element* p_focusElement;
	GUI_Element* p_oldFocusElement;
	GUI_Element* p_mouseDownElement;
	std::string clipboard;
	uint noMoveCounter;
};

#endif