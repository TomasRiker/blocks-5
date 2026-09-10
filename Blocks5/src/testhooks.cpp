#include "pch.h"
#include "testhooks.h"

// testhooks.cpp - the report on the GUI tree that both test paths read.
//
// Everything here is platform-independent: it reads out Engine and GUI and
// builds JSON from that. How the text gets out is elsewhere - in the browser
// in WebBuild/test_hooks.cpp through Module["b5_test"], under Linux at the
// bottom in pollRequests() through a file.

#ifdef BLOCKS5_TEST_HOOKS

#include "engine.h"
#include "upscaler.h"
#include "gamestate.h"
#include "gs_game.h"
#include "gui.h"
#include "gui_element.h"
#include "particlesystem.h"
#include "framestats.h"

#ifndef __EMSCRIPTEN__
#include <cstdio>
#include <unistd.h>
#endif

namespace
{
	void appendEscaped(std::string& out, const std::string& text)
	{
		for(std::string::size_type i = 0; i < text.length(); i++)
		{
			const unsigned char c = static_cast<unsigned char>(text[i]);
			if(c == '"' || c == '\\') { out += '\\'; out += static_cast<char>(c); }
			else if(c < 0x20 || c > 0x7E)
			{
				char buffer[8];
				sprintf(buffer, "\\u%04X", static_cast<unsigned int>(c));
				out += buffer;
			}
			else out += static_cast<char>(c);
		}
	}

	void appendInt(std::string& out, int value)
	{
		char buffer[32];
		sprintf(buffer, "%d", value);
		out += buffer;
	}

	// Two decimals: a frame is milliseconds and the interesting differences
	// here are fractions of one.
	void appendFixed(std::string& out, double value)
	{
		char buffer[32];
		sprintf(buffer, "%.2f", value);
		out += buffer;
	}

	// p50, p95 and the maximum of one phase. Three numbers rather than a mean,
	// because what costs a frame or tears the audio lives in the tail.
	void appendPhase(std::string& out, const char* p_key, FrameStats::Phase phase)
	{
		const FrameStats& stats = Engine::inst().getFrameStats();
		out += ",\"";
		out += p_key;
		out += "\":[";
		appendFixed(out, stats.getPercentile(phase, 50)); out += ",";
		appendFixed(out, stats.getPercentile(phase, 95)); out += ",";
		appendFixed(out, stats.getPercentile(phase, 100));
		out += "]";
	}

	void appendPoint(std::string& out, const char* p_key, int x, int y)
	{
		out += "\"";
		out += p_key;
		out += "\":[";
		appendInt(out, x); out += ",";
		appendInt(out, y); out += "]";
	}

	void appendRect(std::string& out, const char* p_key, int x, int y, int w, int h)
	{
		out += "\"";
		out += p_key;
		out += "\":[";
		appendInt(out, x); out += ",";
		appendInt(out, y); out += ",";
		appendInt(out, w); out += ",";
		appendInt(out, h); out += "]";
	}

	// Game coordinate -> window coordinate, the same arithmetic as in
	// presentFrame(). Engine::getCursorPosition() is the inverse of it: a
	// click on the point it delivers lands on exactly this game coordinate.
	//
	// The CRT filter's curvature stays out of it: it would come from
	// warpToOutput(), which is private, and a test that hits buttons runs
	// without it anyway. Whether something is warping after all is in the
	// JSON as "crt", for a test to notice when that case arises.
	void gameToWindow(const Vec2i& game, int* p_x, int* p_y)
	{
		Engine& engine = Engine::inst();
		const Vec2i screen = engine.getScreenSize();

		int px, py, pw, ph;
		engine.computePresentRect(px, py, pw, ph);
		if(pw <= 0 || ph <= 0 || screen.x <= 0 || screen.y <= 0)
		{
			*p_x = game.x;
			*p_y = game.y;
			return;
		}

		*p_x = px + static_cast<int>((game.x + 0.5) * pw / screen.x);
		*p_y = py + static_cast<int>((game.y + 0.5) * ph / screen.y);
	}

	void dumpElement(std::string& out, GUI_Element* p_element, bool& first)
	{
		if(!p_element) return;

		const Vec2i position = p_element->getAbsPosition();
		const Vec2i size = p_element->getSize();

		int wx, wy, wx2, wy2;
		gameToWindow(position, &wx, &wy);
		gameToWindow(position + size - Vec2i(1, 1), &wx2, &wy2);

		if(!first) out += ",\n";
		first = false;

		out += "  {\"path\":\"";
		appendEscaped(out, p_element->getFullName());
		out += "\",\"type\":\"";
		appendEscaped(out, p_element->getType());
		out += "\",";
		appendRect(out, "rect", position.x, position.y, size.x, size.y);
		out += ",";
		appendRect(out, "win", wx, wy, wx2 - wx + 1, wy2 - wy + 1);
		out += ",\"visible\":";
		out += p_element->isVisible() ? "true" : "false";
		out += ",\"shown\":";
		out += p_element->isReallyVisible() ? "true" : "false";
		out += ",\"active\":";
		out += p_element->isActive() ? "true" : "false";
		out += "}";

		const std::list<GUI_Element*>& children = p_element->getChildren();
		for(std::list<GUI_Element*>::const_iterator i = children.begin(); i != children.end(); ++i)
		{
			dumpElement(out, *i, first);
		}
	}

	std::string buildDump()
	{
		Engine& engine = Engine::inst();

		const Vec2i screen = engine.getScreenSize();
		const Vec2i display = engine.getDisplaySize();

		// The question can come earlier than one thinks - in the browser the
		// export stands ready as soon as the module has loaded, long before
		// main(). Before that screenSize holds a zero, computePresentRect()
		// divides by it, and the cast of the resulting NaN to int is not a
		// wrong value in wasm but a trap: the page hangs instead of reporting
		// an error. Look first whether there is anything to report at all.
		if(screen.x <= 0 || screen.y <= 0 || !GUI::inst().getRoot())
		{
			return std::string("{\"state\":\"\",\"elements\":[]}\n");
		}

		int px, py, pw, ph;
		engine.computePresentRect(px, py, pw, ph);

		GameState* p_state = engine.getGameState();
		GUI_Element* p_focus = GUI::inst().getFocusElement();
		GUI_Element* p_down = GUI::inst().getMouseDownElement();
		const Vec2i cursor(GUI::inst().getCursorPos());

		std::string out("{\n");
		out += "\"state\":\"";
		appendEscaped(out, p_state ? p_state->getName() : "");
		out += "\",\"language\":\"";
		appendEscaped(out, engine.getLanguage());
		out += "\",\"filter\":\"";
		appendEscaped(out, engine.getEffectiveUpscaler()->getName());
		// Not "is the CRT filter on" but "is anything warping the picture":
		// only then are the window coordinates above no longer right. The key
		// is still called crt - WebBuild/test/harness.js reads it that way.
		out += "\",\"crt\":";
		out += engine.getEffectiveUpscaler()->distortsCursor() ? "true" : "false";
		out += ",\"focus\":\"";
		appendEscaped(out, p_focus ? p_focus->getFullName() : "");
		// Where the game sees the cursor and what it is pressing on. From
		// outside, a tap that does not arrive otherwise looks exactly like
		// a button that does not react.
		out += "\",\"appActive\":";
		out += engine.isAppActive() ? "true" : "false";

		// Whether the game is paused. Only a question at all in the game
		// state; told apart by name and not with dynamic_cast, as everywhere
		// in this tree.
		out += ",\"paused\":";
		out += (p_state && p_state->getName() == "GS_Game"
				&& static_cast<GS_Game*>(p_state)->isPaused()) ? "true" : "false";

		// Which named actions are down right now. Only the pressed ones, to
		// keep the report short. That is the only place where it can be seen
		// from outside whether a key reaches the action layer at all - that
		// layer reads SDL_GetKeyState and not keyData, which cannot be told
		// from outside any other way.
		out += ",\"actionsDown\":[";
		{
			const std::vector<Action*>& actions = engine.getActionsVector();
			bool firstAction = true;
			for(std::vector<Action*>::const_iterator i = actions.begin(); i != actions.end(); ++i)
			{
				if(!*i || !engine.isActionDown((*i)->name)) continue;
				if(!firstAction) out += ",";
				firstAction = false;
				out += "\"";
				appendEscaped(out, (*i)->name);
				out += "\"";
			}
		}
		out += "]";

		// The largest number of particles one system has had to draw since the
		// last time this report was asked for, which is what sizes the vertex
		// buffer in ParticleSystem. Reading it clears it, so a test that dumps
		// once at the start of a scene and once at the end gets the peak over
		// exactly that stretch and cannot miss a spike between two polls.
		out += ",\"particlePeak\":";
		appendInt(out, static_cast<int>(ParticleSystem::takePeakCount()));

		// What the frames since the last resetStats() cost, all in
		// milliseconds on the main thread. None of it waits for the GPU -
		// WebGL hands over a command and returns - so these are what the
		// emulation and the JavaScript cost, which is the half that starves
		// the audio and drops the frame.
		{
			const FrameStats& stats = Engine::inst().getFrameStats();
			out += ",\"frames\":{\"count\":";
			appendInt(out, static_cast<int>(stats.getCount()));
			appendPhase(out, "interval", FrameStats::FS_INTERVAL);
			appendPhase(out, "total", FrameStats::FS_TOTAL);
			appendPhase(out, "render", FrameStats::FS_RENDER);
			appendPhase(out, "update", FrameStats::FS_UPDATE);
			appendPhase(out, "present", FrameStats::FS_PRESENT);
			appendPhase(out, "swap", FrameStats::FS_SWAP);
			// Counted against the frame budget, which is the logic rate: a
			// frame whose interval went over is one the player did not get,
			// one whose work went over is one the game is responsible for.
			// Both, because they come apart - a browser at 38 ms a frame on
			// 2.7 ms of work has none of the second and all of the first.
			// over500 is the third question: that is Emscripten's Web Audio
			// lookahead, so a frame past it is a hole in the music.
			const float budget = static_cast<float>(Engine::inst().getLogicRate());
			out += ",\"budgetMs\":";
			appendInt(out, static_cast<int>(budget));
			out += ",\"late\":";
			appendInt(out, static_cast<int>(stats.getCountOver(FrameStats::FS_INTERVAL, budget)));
			out += ",\"lateWork\":";
			appendInt(out, static_cast<int>(stats.getCountOver(FrameStats::FS_TOTAL, budget)));
			out += ",\"over500\":";
			appendInt(out, static_cast<int>(stats.getCountOver(FrameStats::FS_TOTAL, 500.0f)));
			out += "}";
		}

		out += ",\"mouseDown\":\"";
		appendEscaped(out, p_down ? p_down->getFullName() : "");
		out += "\",";
		appendPoint(out, "cursor", cursor.x, cursor.y);
		out += ",";
		appendRect(out, "screen", 0, 0, screen.x, screen.y);
		out += ",";
		appendRect(out, "display", 0, 0, display.x, display.y);
		out += ",";
		appendRect(out, "present", px, py, pw, ph);
		out += ",\n\"elements\":[\n";

		// From the root's children on: the root itself has no name anybody
		// would look it up under.
		bool first = true;
		const std::list<GUI_Element*>& top = GUI::inst().getRoot()->getChildren();
		for(std::list<GUI_Element*>::const_iterator i = top.begin(); i != top.end(); ++i)
		{
			dumpElement(out, *i, first);
		}

		out += "\n]}\n";
		return out;
	}
}

namespace TestHooks
{

std::string dump()
{
	return buildDump();
}

void resetStats()
{
	Engine::inst().getFrameStats().clear();
}

std::string hitAt(int x, int y)
{
	GUI_Element* p_root = GUI::inst().getRoot();
	GUI_Element* p_hit = p_root ? p_root->getElementAt(Vec2i(x, y)) : 0;
	return p_hit ? p_hit->getFullName() : std::string();
}

#ifndef __EMSCRIPTEN__

// In the browser JavaScript calls the export. Natively there is no such
// channel, and the request sits in a file instead: the test writes "request",
// the game reads it once per logic tick, deletes it and puts the answer
// beside it. It is written under another name first and then renamed, because
// that is a single step - the test never sees the answer half finished.
//
// A stat() on a file that does not exist, fifty times a second, costs nothing,
// and this path exists only in the test build.
void pollRequests()
{
	static std::string directory;
	static bool checked = false;
	if(!checked)
	{
		checked = true;
		const char* p_dir = ::getenv("B5_TEST_DIR");
		if(p_dir && *p_dir) directory = p_dir;
	}
	if(directory.empty()) return;

	const std::string requestPath(directory + "/request");
	FILE* p_request = fopen(requestPath.c_str(), "rb");
	if(!p_request) return;

	char line[128] = "";
	if(!fgets(line, sizeof(line), p_request)) line[0] = 0;
	fclose(p_request);
	::remove(requestPath.c_str());

	std::string answer;
	int x = 0, y = 0;
	if(sscanf(line, "hit %d %d", &x, &y) == 2) answer = hitAt(x, y);
	else if(!strncmp(line, "resetstats", 10)) { resetStats(); answer = "ok\n"; }
	else answer = dump();

	const std::string temporaryPath(directory + "/response.tmp");
	FILE* p_response = fopen(temporaryPath.c_str(), "wb");
	if(!p_response) return;
	fwrite(answer.data(), 1, answer.length(), p_response);
	fclose(p_response);
	::rename(temporaryPath.c_str(), (directory + "/response").c_str());
}

#endif // !__EMSCRIPTEN__

}

#endif // BLOCKS5_TEST_HOOKS
