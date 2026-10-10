#include "pch.h"
#include "inputtime.h"
#include "engine.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#elif defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
// Xlib in a translation unit of its own, for the reason engine.cpp gives.
#include "linux_window.h"
#endif

namespace
{
	// An event older than this is taken as one of now: the clock was not the
	// one assumed, or a message sat out a long modal loop.
	const Uint32 MAX_AGE = 10000;
}

#ifdef __EMSCRIPTEN__

void InputTime::beforeSDLInit()
{
	EM_ASM({
		// What Emscripten's SDL queues of a DOM event is the event itself or
		// objects it makes up from it, all in its listener, which SDL_Init
		// registers by reference. Each gets the event's time on SDL_GetTicks'
		// clock. A copy that pre.js or the pad dispatched carries the time of
		// the touch it stands for as b5time, its own being when it was made.
		var maxAge = $0;
		var receive = SDL.receiveEvent;
		SDL.receiveEvent = function(event) {
			var first = SDL.events.length;
			receive.call(SDL, event);
			var stamp = event.b5time !== undefined ? event.b5time : event.timeStamp;
			var age = typeof stamp === "number" ? performance.now() - stamp : 0;
			if (!(age >= 0 && age < maxAge)) age = 0;
			var ticks = Math.max(0, Math.round(Date.now() - SDL.startTime - age));
			for (var i = first; i < SDL.events.length; i++) {
				if (typeof SDL.events[i] === "object") SDL.events[i].b5ticks = ticks;
			}
		};
		// SDL_PollEvent makes the C event of the oldest one here.
		var make = SDL.makeCEvent;
		SDL.makeCEvent = function(event, ptr) {
			SDL.b5ticks = typeof event === "object" && event.b5ticks !== undefined ? event.b5ticks : -1;
			return make.call(SDL, event, ptr);
		};
	}, MAX_AGE);
}

void InputTime::afterSDLInit()
{
}

void InputTime::pumped()
{
}

Uint32 InputTime::ofPolled()
{
	const int ticks = EM_ASM_INT({ return SDL.b5ticks === undefined ? -1 : SDL.b5ticks; });
	return ticks >= 0 ? static_cast<Uint32>(ticks) : SDL_GetTicks();
}

#else

namespace
{
	// When an event that is age milliseconds old happened, on SDL_GetTicks'
	// clock. A time in the future reads as a huge age and counts as now.
	Uint32 ticksAgo(Uint32 age)
	{
		return SDL_GetTicks() - (age < MAX_AGE ? age : 0);
	}

	Uint32 timeOf(const SDL_Event& event);
#ifndef _WIN32
	void noteSystemEvent(const SDL_Event& event);
	bool madeUp(const SDL_Event& event);
#endif

	// SDL calls this for every event it makes, before its queue. Key and
	// mouse events go to the engine instead, with their time, so that the
	// queue they wait in is the engine's own and keeps it.
	int SDLCALL filter(const SDL_Event* p_event)
	{
		switch(p_event->type)
		{
		case SDL_KEYDOWN:
		case SDL_KEYUP:
		case SDL_MOUSEMOTION:
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
#ifndef _WIN32
			if(madeUp(*p_event)) return 0;
#endif
			Engine::inst().queueInput(*p_event, timeOf(*p_event));
			return 0;
#ifndef _WIN32
		case SDL_SYSWMEVENT:
			noteSystemEvent(*p_event);
			return 0;
#endif
		default:
			return 1;
		}
	}
}

void InputTime::beforeSDLInit()
{
}

void InputTime::afterSDLInit()
{
	// SDL 1.2 empties its queue as the filter goes in, which right after
	// SDL_Init holds nothing yet.
	SDL_SetEventFilter(filter);
#ifndef _WIN32
	// What sdl12-compat hands on of the X events - off by default.
	SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
#endif
}

Uint32 InputTime::ofPolled()
{
	// Only what SDL had queued before the filter went in comes this way.
	return SDL_GetTicks();
}

#ifdef _WIN32

namespace
{
	// The time of the input message engineWindowProc is handling, while it is.
	bool messageOpen = false;
	Uint32 messageTime = 0;

	Uint32 timeOf(const SDL_Event&)
	{
		return InputTime::now();
	}
}

void InputTime::pumped()
{
}

InputTime::MessageScope::MessageScope(unsigned int msg) :
	savedOpen(messageOpen),
	savedTime(messageTime)
{
	messageOpen = (msg >= WM_KEYFIRST && msg <= WM_KEYLAST) || (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST);
	// GetMessageTime counts on GetTickCount's clock, in its steps of about
	// 16 ms, which is as fine as the age can be told. SDL_GetTicks is a
	// clock of its own, so the age is what crosses over.
	if(messageOpen) messageTime = ticksAgo(GetTickCount() - static_cast<DWORD>(GetMessageTime()));
}

InputTime::MessageScope::~MessageScope()
{
	messageOpen = savedOpen;
	messageTime = savedTime;
}

Uint32 InputTime::now()
{
	return messageOpen ? messageTime : SDL_GetTicks();
}

#else

namespace
{
	// The time of the input X event the key and mouse events now arriving
	// are made of: set by its SDL_SYSWMEVENT, which comes just before them,
	// and void after an X event of any other kind.
	bool xTimeKnown = false;
	Uint32 xTime = 0;

	// sdl12-compat holds a key-down back until it knows the character the
	// key types, which can be as late as the next key event or the end of
	// the pump, by which time other X events have come. So the times of the
	// presses whose key-down is still to come wait here, oldest first: one
	// held back and the next arriving.
	Uint32 pressTimes[2];
	int numPresses = 0;

	// What X holds down, to tell its repeats - presses SDL drops - from new
	// presses, and the key it let go of last, since a server without
	// detectable autorepeat sends a repeat as a release and a press at once.
	bool xKeyHeld[256];
	unsigned int lastReleasedKey = 0;
	unsigned long lastReleaseTime = 0;

	// What the filter has passed on as held, to tell the repeats
	// sdl12-compat makes up at the end of a pump, which no X event comes
	// before, from the key-downs it held back.
	bool sdlKeyHeld[SDLK_LAST];

	// The X server's clock is CLOCK_MONOTONIC in milliseconds, cut to 32
	// bits, wherever the server runs on this machine. One elsewhere gives
	// ages past MAX_AGE, which count as none.
	Uint32 ticksOfX(unsigned long time)
	{
		timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		const Uint32 nowMs = static_cast<Uint32>(now.tv_sec) * 1000 + static_cast<Uint32>(now.tv_nsec / 1000000);
		return ticksAgo(nowMs - static_cast<Uint32>(time));
	}

	void noteSystemEvent(const SDL_Event& event)
	{
		unsigned int keycode = 0;
		unsigned long time = 0;
		const LinuxWindow::XEventKind kind = LinuxWindow::readInputEvent(event, &keycode, &time);
		xTimeKnown = kind != LinuxWindow::XE_NONE && kind != LinuxWindow::XE_FOCUS_OUT;
		// X sends no release for a key let go of in another window.
		if(kind == LinuxWindow::XE_FOCUS_OUT) std::fill(xKeyHeld, xKeyHeld + 256, false);
		if(!xTimeKnown) return;
		xTime = ticksOfX(time);

		if(kind == LinuxWindow::XE_KEY_RELEASE)
		{
			if(keycode < 256) xKeyHeld[keycode] = false;
			lastReleasedKey = keycode;
			lastReleaseTime = time;
		}
		else if(kind == LinuxWindow::XE_KEY_PRESS)
		{
			const bool repeat = keycode < 256 &&
								(xKeyHeld[keycode] || (keycode == lastReleasedKey && time - lastReleaseTime < 2));
			if(keycode < 256) xKeyHeld[keycode] = true;
			if(repeat) return;
			if(numPresses == 2)
			{
				pressTimes[0] = pressTimes[1];
				numPresses = 1;
			}
			pressTimes[numPresses++] = xTime;
		}
	}

	// Whether a key-down is a repeat sdl12-compat made up for a key already
	// let go of. A release in the same pump as its press stops the repeat
	// before it lets the held-back press through, which starts it again, so
	// the key would repeat until another one is pressed. SDL's own keyboard
	// says the key is up, and no X press waits for a key-down.
	bool madeUp(const SDL_Event& event)
	{
		if(event.type != SDL_KEYDOWN || numPresses > 0) return false;
		int numKeys = 0;
		const Uint8* p_keys = SDL_GetKeyState(&numKeys);
		const int sym = event.key.keysym.sym;
		return sym >= 0 && sym < numKeys && !p_keys[sym];
	}

	Uint32 timeOf(const SDL_Event& event)
	{
		const int sym = event.type == SDL_KEYDOWN || event.type == SDL_KEYUP ? event.key.keysym.sym : -1;
		const bool symKnown = sym >= 0 && sym < SDLK_LAST;

		if(event.type == SDL_KEYDOWN)
		{
			const bool repeat = symKnown && sdlKeyHeld[sym];
			if(symKnown) sdlKeyHeld[sym] = true;
			if(repeat) return SDL_GetTicks();
			if(numPresses > 0)
			{
				const Uint32 time = pressTimes[0];
				pressTimes[0] = pressTimes[1];
				numPresses--;
				return time;
			}
		}
		else if(event.type == SDL_KEYUP && symKnown) sdlKeyHeld[sym] = false;

		return xTimeKnown ? xTime : SDL_GetTicks();
	}
}

void InputTime::pumped()
{
	// A pump ends with every held-back key-down let through, so whatever
	// press is left over had none, and no X event is being turned into
	// events any more.
	numPresses = 0;
	xTimeKnown = false;
}

#endif

#endif
