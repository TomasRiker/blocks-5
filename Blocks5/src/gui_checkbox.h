#ifndef _GUI_CHECKBOX_H
#define _GUI_CHECKBOX_H

/*** Klasse für eine Check-Box ***/

#include "gui_element.h"

class GUI_CheckBox : public GUI_Element
{
public:
	DECL_CTOR(GUI_CheckBox);
	~GUI_CheckBox();

	void onRender();
	void onMouseDown(const Vec2i& position, int buttons);
	void onMouseUp(const Vec2i& position, int buttons);
	void onMouseEnter(int buttons);
	void onMouseLeave(int buttons);
	INLINE_GETTYPE("GUI_CheckBox");

	void readAttributes(TiXmlElement* p_element);

	INLINE_GETTER(std::string, getTitle, title);
	INLINE_SETTER(std::string, setTitle, title);
	INLINE_GETTER(bool, isChecked, checked);
	// check() ist der Benutzerklick: es loest das changed-Signal aus.
	// setChecked() ist das Nachziehen der Anzeige und tut das nicht - wer
	// eine Anzeige aktualisiert, meint keine Eingabe.
	//
	// Nur checked, nie newChecked: newChecked ist der laufende Klick. Es wird
	// in onMouseDown gesetzt und in onMouseUp gelesen, und dazwischen laeuft
	// ein Bild. Wer es hier mitzieht, loescht den gedrueckten, noch nicht
	// losgelassenen Klick - im Leveleditor sah das so aus, als spraenge die
	// Elektrizitaet sofort wieder aus.
	void check(bool check);
	void setChecked(bool check) { checked = check; }

	INLINE_CONNECTOR(connectChanged, changed);

private:
	std::string title;
	bool checked;
	bool newChecked;
	bool pushed;
	bool mouseOver;

	sigslot::signal1<GUI_Element*> changed;
};

#endif