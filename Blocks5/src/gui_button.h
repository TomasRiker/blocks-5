#ifndef _GUI_BUTTON_H
#define _GUI_BUTTON_H

/*** Klasse fuer einen Button ***/

#include "gui_element.h"

class Texture;

class GUI_Button : public GUI_Element
{
public:
	DECL_CTOR(GUI_Button);
	~GUI_Button();

	bool containsPoint(const Vec2i& position);
	void onRender();
	void onUpdate();
	void onMouseDown(const Vec2i& position, int buttons);
	void onMouseUp(const Vec2i& position, int buttons);
	void onMouseEnter(int buttons);
	void onMouseLeave(int buttons);
	INLINE_GETTYPE("GUI_Button");

	void click();

	void readAttributes(TiXmlElement* p_element);

	INLINE_GETTER(std::string, getTitle, title);
	INLINE_SETTER(std::string, setTitle, title);

	INLINE_GETTER(std::string, getImageFilename, imageFilename);
	void setImageFilename(const std::string& imageFilename);
	// Der Name, wie er in der XML steht. Ist es eine $ID, kann er je nach
	// Sprache auf ein anderes Bild zeigen; onUpdate loest ihn deshalb neu auf.
	void setRawImageFilename(const std::string& rawImageFilename);
	INLINE_GETTER(Vec2i, getPositionOnTexture, positionOnTexture);
	INLINE_SETTER(Vec2i, getPositionOnTexture, positionOnTexture);
	INLINE_GETTER(Vec2i, getClickedPositionOnTexture, clickedPositionOnTexture);
	INLINE_SETTER(Vec2i, getClickedPositionOnTexture, clickedPositionOnTexture);

	INLINE_CONNECTOR(connectClicked, clicked);

private:
	std::string title;
	bool pushed;
	bool mouseOver;

	int style;

	// Wie viele Pixel des Feldes ringsum nur Rand sind. Ein Feld in
	// buttons.png ist groesser als die Scheibe darin - der Rest gehoert zum
	// Schlagschatten und ist durchsichtig. Ohne diesen Abzug waere ein Knopf
	// auch dort anklickbar, wo nichts zu sehen ist, und in der Levelauswahl
	// griffen benachbarte Knoepfe einander in die Scheibe.
	int imageInset;

	std::string imageFilename;
	std::string rawImageFilename;
	Vec2i positionOnTexture;
	Vec2i clickedPositionOnTexture;
	Vec4d stdColor;
	Vec4d hoverColor;
	Vec4d currentColor;
	double stdScaling;
	double hoverScaling;
	double currentScaling;
	Texture* p_image;

	sigslot::signal1<GUI_Element*> clicked;
};

#endif