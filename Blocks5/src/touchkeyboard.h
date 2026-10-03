#ifndef _TOUCHKEYBOARD_H
#define _TOUCHKEYBOARD_H

/*** The system's touch keyboard ***/

// Under Windows the touch keyboard, asked for when a finger or a pen taps a
// text field (GUI::fingerFocused) and sent away when the focus leaves the
// text fields: the game's window is no text control, so Windows never opens
// it by itself. It is shown only on a tablet in tablet posture, as Windows
// does for its own fields, and what happened goes into the log. Everywhere
// else both do nothing - the browser types through its text sheet
// (web_textsheet.cpp).
namespace TouchKeyboard
{
	void show();
	void hide();
}

#endif
