#ifndef _FATALERROR_H
#define _FATALERROR_H

/*** The one way the game gives up ***/

// Put a message in front of the player and end the program. Does not return.
//
// printfLog() alone reaches nobody here: a Release build is a Windows-subsystem
// binary with no console, a Linux player starts the game from a launcher, and a
// browser keeps its console behind the developer tools. So the few things worth
// stopping for need a window of their own.
//
// Implemented three times in fatalerror.cpp - a Win32 message box, zenity or
// kdialog under Linux (the pair the file dialogs already reach for), a DOM
// overlay in the browser - because that is the whole of what differs. The
// message itself is composed once, by the caller.
void fatalError(const std::string& title, const std::string& message);

#endif
