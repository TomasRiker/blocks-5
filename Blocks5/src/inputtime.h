#ifndef _INPUTTIME_H
#define _INPUTTIME_H

/*** When a key or mouse event happened ***/

// SDL 1.2 puts no time on an event, and the main loop drains a frame's events
// at once, so every tick a slow frame catches up on would see the input as it
// stood at the end of that frame. Engine::replayInput hands each tick the
// input up to its own moment instead, and for that it needs the moment each
// event happened, on SDL_GetTicks' clock. Each platform keeps it somewhere of
// its own:
//
//   Windows  the time of the window message SDL makes the event of, which
//            engineWindowProc brackets SDL's procedure with (MessageScope)
//   Linux    the X event's own time, which sdl12-compat hands on as an
//            SDL_SYSWMEVENT just before the events it makes of it
//   browser  the DOM event's timeStamp, copied onto what Emscripten's SDL
//            queues by a wrapper around its listener
//
// Where there is none - an event SDL makes up itself, a key repeat or the
// mouse position it reads at the end of a pump - the time is when SDL made it.
namespace InputTime
{
	// Before SDL_Init, which hands the browser the listener this wraps.
	void beforeSDLInit();
	// Right after SDL_Init: natively the event filter that hands every key
	// and mouse event to Engine::queueInput the moment SDL makes it, and
	// keeps it out of SDL's own queue.
	void afterSDLInit();
	// SDL has finished pumping: what the pump left half-known is forgotten.
	void pumped();
	// The time of the key or mouse event SDL_PollEvent just returned. In the
	// browser, whose SDL has no filter, every one comes that way.
	Uint32 ofPolled();

#ifdef _WIN32
	// While one stands, the events SDL makes take the time of the window
	// message it was made for, if that is an input message. engineWindowProc
	// makes one for every message, and one nested inside puts the outer back.
	class MessageScope
	{
	public:
		explicit MessageScope(unsigned int msg);
		~MessageScope();

	private:
		bool savedOpen;
		Uint32 savedTime;
	};

	// The time of an event made now: the message's, inside a MessageScope.
	Uint32 now();
#endif
}

#endif
