#include "pch.h"
#include "updatecheck.h"

// Nothing at all in the browser; updatecheck.h says why.
#ifndef __EMSCRIPTEN__

#ifdef _WIN32
#include <wininet.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#endif

extern const char* p_localVersion;

namespace
{
	// How long the connection may take, handed to WinINet, curl and wget
	// alike. The game gives up two seconds later, so that a tool keeping to
	// its own limit ends with its own error rather than being killed.
	const uint TIMEOUT_SECONDS = 10;
	const uint GIVE_UP_MS = (TIMEOUT_SECONDS + 2) * 1000;

	// An answer this long is no version number, whatever follows: three
	// groups of three digits and a line break fit in thirteen bytes.
	const size_t MAX_ANSWER = 16;

	UpdateCheck::State state = UpdateCheck::STATE_IDLE;
	std::string newVersion;
	uint startTicks = 0;

	// Empty for the game's own; see getVersion().
	std::string versionOverride;

	// The version in the agent string tells the server log which version is
	// asking.
	std::string userAgent()
	{
		return std::string("Scherfgen-Software Blocks 5 (") + UpdateCheck::getVersion() + ")";
	}

	std::string versionURL()
	{
#ifdef BLOCKS5_TEST_HOOKS
		// A server of the harness's own, which answers whatever a test needs:
		// a newer version, the same one, garbage, an error or nothing at all.
		const char* p_url = getenv("B5_UPDATE_URL");
		if(p_url && *p_url) return p_url;
#endif
		return "https://www.david-scherfgen.de/stuff/blocks-5/version.txt";
	}

	// "1.2.0" to 1002000, and anything that is not a version number to -1.
	// Up to three groups, a missing one counts as 0, whitespace before and
	// after is allowed.
	long parseVersion(const std::string& text)
	{
		size_t i = 0;
		while(i < text.length() && isspace(static_cast<unsigned char>(text[i]))) ++i;

		long part[3] = { 0, 0, 0 };
		int n = 0;
		bool anyDigit = false;
		while(n < 3)
		{
			if(i >= text.length() || !isdigit(static_cast<unsigned char>(text[i]))) break;
			long value = 0;
			while(i < text.length() && isdigit(static_cast<unsigned char>(text[i])))
			{
				value = value * 10 + (text[i++] - '0');
				if(value > 999) return -1;
			}
			part[n++] = value;
			anyDigit = true;
			if(i < text.length() && text[i] == '.') ++i;
			else break;
		}

		while(i < text.length() && isspace(static_cast<unsigned char>(text[i]))) ++i;
		if(!anyDigit || i != text.length()) return -1;

		return part[0] * 1000000 + part[1] * 1000 + part[2];
	}

	void fail(const char* p_why)
	{
		printfLog("Update check failed: %s\n", p_why);
		state = UpdateCheck::STATE_FAILED;
	}

	// The state the server's answer leads to. Compared as numbers, not
	// strings: a trailing line break, or "1.10.0" against "1.9.0", would
	// otherwise offer an update to somebody who has the newest. What is not a
	// version number - an error page, "1.3.0-beta" - fails the check rather
	// than passing for the newest.
	void conclude(const std::string& answer)
	{
		const long latest = parseVersion(answer);
		if(latest < 0)
		{
			fail("the answer is no version number");
		}
		else if(latest > parseVersion(UpdateCheck::getVersion()))
		{
			// For the tooltip, without the line break around it.
			const size_t first = answer.find_first_not_of(" \t\r\n");
			const size_t last = answer.find_last_not_of(" \t\r\n");
			newVersion = answer.substr(first, last - first + 1);
			printfLog("Update check: version %s is available.\n", newVersion.c_str());
			state = UpdateCheck::STATE_AVAILABLE;
		}
		else
		{
			printfLog("Update check: up to date.\n");
			state = UpdateCheck::STATE_UP_TO_DATE;
		}
	}

	bool timedOut()
	{
		// Unsigned, so a tick count that wraps round in between still works.
		return SDL_GetTicks() - startTicks >= GIVE_UP_MS;
	}

#ifdef _WIN32
	// One check's thread and what it hands back. Freed by whichever of the two
	// lets go last, the game or the thread: the game gives up after
	// GIVE_UP_MS, and the thread may still be about to write its answer then.
	struct Task
	{
		Task(const std::string& url, const std::string& agent) : url(url), agent(agent), refs(2)
		{
		}

		static void release(Task* p_task)
		{
			if(InterlockedDecrement(&p_task->refs) == 0) delete p_task;
		}

		static DWORD WINAPI threadProc(void* p_param)
		{
			Task* p_task = reinterpret_cast<Task*>(p_param);
			p_task->answer = p_task->fetch();
			release(p_task);
			return 0;
		}

		// What the server sent, or "" when anything went wrong.
		std::string fetch() const
		{
			HINTERNET inet = InternetOpenA(agent.c_str(), INTERNET_OPEN_TYPE_PRECONFIG, 0, 0, 0);
			if(!inet) return "";

			// WinINet's own limits run to minutes, and a thread the game has
			// given up on would hang on for them.
			DWORD timeout = TIMEOUT_SECONDS * 1000;
			InternetSetOptionA(inet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
			InternetSetOptionA(inet, INTERNET_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));
			InternetSetOptionA(inet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

			// The https scheme implies INTERNET_FLAG_SECURE; written out, it
			// says the https is deliberate.
			HINTERNET request = InternetOpenUrlA(inet, url.c_str(), 0, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE, 0);
			std::string answer;
			bool complete = false;
			if(request)
			{
				char buffer[MAX_ANSWER];
				DWORD numBytesRead = 0;
				while(InternetReadFile(request, buffer, sizeof(buffer), &numBytesRead))
				{
					// Zero bytes from a successful read is the end of the file.
					if(numBytesRead == 0)
					{
						complete = true;
						break;
					}
					answer.append(buffer, numBytesRead);
					if(answer.length() >= MAX_ANSWER) break;
				}
				InternetCloseHandle(request);
			}
			InternetCloseHandle(inet);
			return complete ? answer : "";
		}

		const std::string url;
		const std::string agent;
		std::string answer;
		volatile LONG refs;
	};

	Task* p_task = 0;
	HANDLE thread = 0;

	// Lets go of the running check, whether its thread has ended or not.
	void endTask()
	{
		CloseHandle(thread);
		thread = 0;
		Task::release(p_task);
		p_task = 0;
	}
#else
	// The tool's process and the read end of what it writes.
	pid_t pid = -1;
	int fd = -1;
	std::string output;

	// Closes the pipe and reaps the process; its exit code, or -1 where it did
	// not exit on its own. One being given up is killed first, or waitpid()
	// would wait for it.
	int endProcess(bool giveUp)
	{
		if(giveUp) ::kill(pid, SIGKILL);
		::close(fd);
		int status = 0;
		while(::waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
		pid = -1;
		fd = -1;
		return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
	}
#endif
}

bool UpdateCheck::isPossible()
{
#ifdef _WIN32
	return true;
#else
	return haveProgram("curl") || haveProgram("wget");
#endif
}

void UpdateCheck::start()
{
	if(state == STATE_CHECKING) return;

	printfLog("Checking for updates ...\n");
	newVersion = "";

#ifdef _WIN32
	Task* p_newTask = new Task(versionURL(), userAgent());
	DWORD threadID;
	thread = CreateThread(0, 0, Task::threadProc, p_newTask, 0, &threadID);
	if(!thread)
	{
		delete p_newTask;
		fail("no thread");
		return;
	}
	p_task = p_newTask;
#else
	// Everything the child needs is built before the fork: the game has
	// threads, and between fork() and exec() the child may only make
	// async-signal-safe calls, which an allocation is not. The tool is run
	// directly, with no shell in between, so nothing needs quoting.
	const std::string agent(userAgent()), url(versionURL());
	char seconds[16];
	sprintf(seconds, "%u", TIMEOUT_SECONDS);

	int fds[2];
	if(::pipe(fds) != 0)
	{
		fail("no pipe");
		return;
	}

	const pid_t child = ::fork();
	if(child < 0)
	{
		::close(fds[0]);
		::close(fds[1]);
		fail("no process");
		return;
	}
	if(child == 0)
	{
		::dup2(fds[1], STDOUT_FILENO);
		::close(fds[0]);
		::close(fds[1]);

		// The tools' complaints do not belong in the game's terminal; the
		// menu says that the check failed.
		const int devNull = ::open("/dev/null", O_WRONLY);
		if(devNull >= 0)
		{
			::dup2(devNull, STDERR_FILENO);
			if(devNull != STDERR_FILENO) ::close(devNull);
		}

		// curl where it is installed, wget where not: execlp() returns only
		// where the program is missing. -f makes an HTTP error an exit code
		// rather than an error page on stdout, and -L follows a redirect, as
		// wget and WinINet do of their own accord.
		::execlp("curl", "curl", "-fsL", "--max-time", seconds, "-A", agent.c_str(),
				 url.c_str(), static_cast<char*>(0));
		::execlp("wget", "wget", "-q", "-T", seconds, "-t", "1", "-U", agent.c_str(),
				 "-O", "-", url.c_str(), static_cast<char*>(0));
		::_exit(127);
	}

	::close(fds[1]);
	// Non-blocking, or poll()'s read() would stop the game until the tool is
	// done.
	::fcntl(fds[0], F_SETFL, ::fcntl(fds[0], F_GETFL, 0) | O_NONBLOCK);
	fd = fds[0];
	pid = child;
	output = "";
#endif

	startTicks = SDL_GetTicks();
	state = STATE_CHECKING;
}

void UpdateCheck::poll()
{
	if(state != STATE_CHECKING) return;

#ifdef _WIN32
	// The answer counts only once the thread is seen to have ended: then its
	// writes are done, and the wait is what makes them visible here.
	if(WaitForSingleObject(thread, 0) == WAIT_OBJECT_0)
	{
		const std::string answer(p_task->answer);
		endTask();
		if(answer.empty()) fail("no answer from the server");
		else conclude(answer);
		return;
	}
#else
	for(;;)
	{
		char buffer[64];
		const ssize_t numBytesRead = ::read(fd, buffer, sizeof(buffer));
		if(numBytesRead > 0)
		{
			output.append(buffer, numBytesRead);
			if(output.length() >= MAX_ANSWER)
			{
				endProcess(true);
				fail("the answer is too long for a version number");
				return;
			}
		}
		else if(numBytesRead == 0)
		{
			// The end of the pipe: the tool is done, and its exit code says
			// whether what it wrote is the file. 127 is the child above
			// finding neither tool.
			const int exitCode = endProcess(false);
			if(exitCode == 127) fail("neither curl nor wget is installed");
			else if(exitCode != 0) fail("no answer from the server");
			else conclude(output);
			return;
		}
		else if(errno == EAGAIN || errno == EWOULDBLOCK) break;
		else if(errno != EINTR)
		{
			endProcess(true);
			fail("the pipe broke");
			return;
		}
	}
#endif

	// Asked after the answer, so that one which came in time but was not
	// polled until later still counts.
	if(timedOut())
	{
#ifdef _WIN32
		endTask();
#else
		endProcess(true);
#endif
		fail("no answer in time");
	}
}

void UpdateCheck::abort()
{
	if(state != STATE_CHECKING) return;

#ifdef _WIN32
	endTask();
#else
	endProcess(true);
#endif
	state = STATE_IDLE;
}

UpdateCheck::State UpdateCheck::getState()
{
	return state;
}

const std::string& UpdateCheck::getNewVersion()
{
	return newVersion;
}

void UpdateCheck::setVersion(const std::string& version)
{
	if(parseVersion(version) < 0)
	{
		printfLog("Update check: \"%s\" is no version number, so the game's own stands.\n", version.c_str());
		return;
	}
	printfLog("Update check: taking %s for the version running.\n", version.c_str());
	versionOverride = version;
}

const std::string& UpdateCheck::getVersion()
{
	static const std::string own(p_localVersion);
	if(versionOverride.empty()) return own;
	return versionOverride;
}

#endif
