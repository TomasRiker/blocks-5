#ifndef _OPTIONS_H
#define _OPTIONS_H

#include "gui_element.h"

class Options : public GUI_Element, public sigslot::has_slots<>
{
public:
	Options(GUI_Element* p_parent);
	~Options();

	void show(GUI_Element* p_focusWhenClosed = 0);
	// Escape = Abbrechen, Return = OK. Steht das Roehrenfenster offen,
	// schliesst beides erst einmal nur dieses.
	void onKeyEvent(const SDL_KeyboardEvent& event);

	// Holt das Ergebnis ab, wenn ein Tastenknopf auf eine Taste wartet. Die
	// Wartestellung liegt in der Engine und laeuft nebenher; hier wird nur
	// jeden Takt nachgesehen, ob sie fertig ist.
	void onUpdate();

	void handleClick(GUI_Element* p_element);

private:
	void applyKeyGrab(int key);

	GUI_Element* p_focusWhenClosed;
	bool changed;

	// Welcher Tastenknopf gerade wartet ("PrimaryKey"/"SecondaryKey"), und
	// fuer welche Aktion. Leer heisst: keiner.
	std::string grabButton;
	std::string grabAction;
};

#endif