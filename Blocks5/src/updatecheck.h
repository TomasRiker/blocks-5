#ifndef _UPDATECHECK_H
#define _UPDATECHECK_H

/*** Asking the website whether a newer version is out ***/

// In the background, so nothing waits for the network: a thread under Windows
// and curl or wget as a process of its own under Linux, which keeps TLS out of
// the game for a sixteen-byte answer. start() sets it off and poll(), asked
// once a tick by whatever shows the state, takes the answer when there is one.
//
// None of it exists in the browser: the page is always the newest version, so
// there is nothing to ask, and a call from code that runs there is a compile
// error rather than a check that quietly never answers.

#ifndef __EMSCRIPTEN__

namespace UpdateCheck
{
	enum State
	{
		STATE_IDLE = 0,		// nothing asked yet
		STATE_CHECKING,
		STATE_UP_TO_DATE,
		STATE_AVAILABLE,	// getNewVersion() names it
		STATE_FAILED		// no answer in time, or one that is no version number
	};

	// Whether this machine can ask at all: always under Windows, under Linux
	// only where curl or wget is installed. Asks the shell, so not per tick.
	bool isPossible();

	// Sets a check off, unless one is running. A check that cannot even be
	// started ends in STATE_FAILED at once.
	void start();

	// Takes the answer once it is there and gives up on a check that has run
	// out of time. Cheap enough for every tick, and late is no harm: an answer
	// that came while nobody asked is still taken.
	void poll();

	// Gives up on a running check, for the end of the program: under Linux
	// the process is killed and reaped, under Windows the thread is left to
	// finish on its own.
	void abort();

	State getState();
	const std::string& getNewVersion();

	// The version the check takes for the one running: what it compares the
	// answer with, what its agent string names and what the menu's button
	// shows. The game's own unless -updatecheckversion says otherwise
	// (main.cpp), which is how an offered update can be seen without
	// publishing one. Nothing else takes it, .initialized least of all.
	// setVersion() ignores what is not a version number.
	void setVersion(const std::string& version);
	const std::string& getVersion();
}

#endif

#endif
