#include "pch.h"
#include "fatalerror.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#elif defined(_WIN32)
#include <Windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

void fatalError(const std::string& title, const std::string& message)
{
	// Into the log first, whatever becomes of the dialog: the log is the part
	// somebody can send on.
	printfLog("FATAL: %s\n%s\n", title.c_str(), message.c_str());

#ifdef __EMSCRIPTEN__

	// A single apostrophe inside EM_ASM is a broken character literal, since
	// the body goes through the C preprocessor - double quotes only.
	//
	// The overlay hangs off <body> and needs no exitFullscreen(): this game
	// puts the fullscreen on the root element, so everything on the page stays
	// inside it. textContent and not innerHTML - the message carries driver
	// strings this code did not write.
	EM_ASM({
		var box = document.createElement("div");
		box.setAttribute("style",
			"position:fixed;left:0;top:0;width:100%;height:100%;z-index:2147483647;" +
			"background:#101820;color:#d0d8e0;font:16px monospace;padding:24px;" +
			"box-sizing:border-box;white-space:pre-wrap;overflow:auto");
		var head = document.createElement("div");
		head.setAttribute("style", "font-size:20px;font-weight:bold;color:#ff9090;margin-bottom:16px");
		head.textContent = UTF8ToString($0);
		var body = document.createElement("div");
		body.textContent = UTF8ToString($1);
		box.appendChild(head);
		box.appendChild(body);
		document.body.appendChild(box);
	}, title.c_str(), message.c_str());

#elif defined(_WIN32)

	// MB_SETFOREGROUND and MB_TOPMOST because the caller may already have put
	// the game into its WS_POPUP fullscreen, and a box behind that one is a
	// hang as far as the player can tell.
	MessageBoxA(0, message.c_str(), title.c_str(),
				MB_OK | MB_ICONERROR | MB_SETFOREGROUND | MB_TOPMOST);

#else

	// zenity under GNOME, kdialog under KDE - the pair transfer.cpp already
	// depends on for the file dialogs. execlp() and not system(): the message
	// carries strings the graphics driver wrote, and an argument handed
	// straight to the program needs no quoting and cannot become a command.
	// zenity does read its text as Pango markup, though, and a shader log
	// holds < and &: unescaped, the box would come up empty.
	std::string markup;
	for(size_t i = 0; i < message.length(); i++)
	{
		if(message[i] == '&') markup += "&amp;";
		else if(message[i] == '<') markup += "&lt;";
		else if(message[i] == '>') markup += "&gt;";
		else markup += message[i];
	}

	bool shown = false;
	for(int attempt = 0; attempt < 2 && !shown; attempt++)
	{
		const pid_t pid = fork();
		if(pid < 0) break;
		if(pid == 0)
		{
			if(attempt == 0)
			{
				execlp("zenity", "zenity", "--error",
					   "--title", title.c_str(), "--text", markup.c_str(),
					   static_cast<char*>(0));
			}
			else
			{
				execlp("kdialog", "kdialog",
					   "--title", title.c_str(), "--error", message.c_str(),
					   static_cast<char*>(0));
			}
			// execlp() returns only where the program is not installed.
			_exit(127);
		}

		int status = 0;
		if(waitpid(pid, &status, 0) < 0) break;
		// 127 is the child above saying so. Every other end - including the
		// player closing the box with the window button, which zenity reports
		// as 1 - means the message was on the screen.
		shown = !(WIFEXITED(status) && WEXITSTATUS(status) == 127);
	}

	// Neither installed. A terminal is then the only thing left, and somebody
	// who started the game from one will see it.
	if(!shown) fprintf(stderr, "%s\n%s\n", title.c_str(), message.c_str());

#endif

	exit(1);
}
