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
#include "level.h"
#include "gui.h"
#include "gui_element.h"
#include "gui_button.h"
#include "particlesystem.h"
#include "framestats.h"
#include "font.h"

#ifndef __EMSCRIPTEN__
#include <cstdio>
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
		appendEscaped(out, engine.getUpscaler()->getName());
		// Not "is the CRT filter on" but "is anything warping the picture":
		// only then are the window coordinates above no longer right. The key
		// is still called crt - WebBuild/test/harness.js reads it that way.
		out += "\",\"crt\":";
		out += engine.getUpscaler()->distortsCursor() ? "true" : "false";
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

		// The logic tick, in milliseconds. A test that wants a named frame
		// waits for this rather than for an interval: it is what the seeded
		// generator is keyed on, so two runs agree on the picture only where
		// they agree on this.
		out += ",\"time\":";
		appendInt(out, static_cast<int>(engine.getTime()));
		out += ",\"scene\":";
		appendInt(out, static_cast<int>(engine.sceneTick));
		out += ",\"frozen\":";
		out += TestHooks::frozen() ? "true" : "false";

		// What the renderer did since the last resetstats.
		const Renderer::Stats& batch = Renderer::inst().stats();
		out += ",\"batch\":{\"flushes\":";
		appendInt(out, static_cast<int>(batch.flushes));
		out += ",\"draws\":";
		appendInt(out, static_cast<int>(batch.draws));
		out += ",\"quads\":";
		appendInt(out, static_cast<int>(batch.quads));
		// The flushes that drew something, by what asked for them. The names
		// follow Renderer::FlushReason in order.
		static const char* const REASONS[Renderer::FR_COUNT] =
			{"texture", "blend", "scope", "full", "explicit", "frame", "direct"};
		out += ",\"byReason\":{";
		for(int i = 0; i < Renderer::FR_COUNT; i++)
		{
			if(i) out += ",";
			out += "\"";
			out += REASONS[i];
			out += "\":";
			appendInt(out, static_cast<int>(batch.drawsByReason[i]));
		}
		out += "}}";
		// Every draw call render() made, over the frames it made them in -
		// the number a renderer change is measured by, where the batch's own
		// count above sees only its own flushes. The count is native only:
		// it comes from the wrappers at the foot of this file, and in the
		// browser the harness counts on the WebGL context instead - so the
		// key is left out there rather than reported as a zero, which would
		// read as a frame that drew nothing.
		out += ",\"draws\":{\"frames\":";
		appendInt(out, static_cast<int>(engine.renderedFrames));
#ifndef __EMSCRIPTEN__
		out += ",\"calls\":";
		appendInt(out, static_cast<int>(engine.renderDraws));
#endif
		out += "}";

		// What the font's string cache did with the same frames. quads is what
		// it is holding right now and not a count since the reset - that is
		// the number the budget is spent in, 64 bytes of glyph geometry to a
		// character. measures counts every call to measureText(), measureHits
		// those a laid-out string answered and dimHits those the dimensions
		// cache did; what is left over are the walks.
		const Font::CacheStats& fc = Font::getCacheStats();
		out += ",\"fontcache\":{\"hits\":";
		appendInt(out, static_cast<int>(fc.hits));
		out += ",\"misses\":";
		appendInt(out, static_cast<int>(fc.misses));
		out += ",\"evictions\":";
		appendInt(out, static_cast<int>(fc.evictions));
		out += ",\"measures\":";
		appendInt(out, static_cast<int>(fc.measures));
		out += ",\"measureHits\":";
		appendInt(out, static_cast<int>(fc.measureHits));
		out += ",\"dimHits\":";
		appendInt(out, static_cast<int>(fc.dimHits));
		out += ",\"dimEvictions\":";
		appendInt(out, static_cast<int>(fc.dimEvictions));
		out += ",\"dimEntries\":";
		appendInt(out, static_cast<int>(fc.dimEntries));
		out += ",\"dimBytes\":";
		appendInt(out, static_cast<int>(fc.dimBytes));
		out += ",\"entries\":";
		appendInt(out, static_cast<int>(fc.entries));
		out += ",\"quads\":";
		appendInt(out, static_cast<int>(fc.quads));
		out += "}";

		// What the frames since the last resetStats() cost, all in
		// milliseconds on the main thread. None of it waits for the GPU -
		// WebGL hands over a command and returns - so these are what the
		// game and the JavaScript cost, which is the half that starves
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

uint drawCalls = 0;

std::string dump()
{
	return buildDump();
}

namespace
{
	// ~0u for "never". A tick and not a countdown, so two runs that reach it
	// after a different number of rendered frames still stop in the same
	// place.
	uint freezeTick = ~0u;
	// The same for a crossfade's own clock, in milliseconds of its progress.
	uint freezeFadeMs = ~0u;
	bool isFrozen = false;
	bool frameDue = false;
}

void freezeAt(uint tick)
{
	freezeTick = tick;
	freezeFadeMs = ~0u;
	isFrozen = false;
	frameDue = false;
}

void freezeAtFade(uint ms)
{
	freezeFadeMs = ms;
	freezeTick = ~0u;
	isFrozen = false;
	frameDue = false;
}

void checkFreeze(uint tick, int fadeMs)
{
	if(isFrozen) return;
	const bool fadeReached = (freezeFadeMs != ~0u && fadeMs >= 0 &&
							  static_cast<uint>(fadeMs) >= freezeFadeMs);
	if(tick >= freezeTick || fadeReached)
	{
		isFrozen = true;
		frameDue = true;
	}
}

bool frozen()
{
	return isFrozen;
}

bool frozenFrameDue()
{
	const bool due = frameDue;
	frameDue = false;
	return due;
}

namespace
{
	bool lockstepOn = false;
}

void setLockstep(bool on)
{
	lockstepOn = on;
}

bool lockstep()
{
	return lockstepOn;
}

void resetStats()
{
	Engine& engine = Engine::inst();
	engine.getFrameStats().clear();
	Font::resetCacheStats();
	Renderer::inst().resetStats();
	engine.renderDraws = 0;
	engine.renderedFrames = 0;
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

	char buffer[128] = "";
	if(!fgets(buffer, sizeof(buffer), p_request)) buffer[0] = 0;
	fclose(p_request);
	::remove(requestPath.c_str());

	// "#<serial> <request>": the serial goes back on the answer's first
	// line, which is how the harness tells the answer to this request from
	// a late one to an earlier request it had given up on.
	std::string serial;
	const char* line = buffer;
	if(*line == '#')
	{
		const char* p_space = strchr(line, ' ');
		if(p_space)
		{
			serial.assign(line, p_space - line);
			line = p_space + 1;
		}
	}

	// The rest of the line after a keyword, without the newline fgets
	// leaves on.
	struct Argument
	{
		static std::string of(const char* p_text)
		{
			std::string s(p_text);
			while(!s.empty() && (s[s.length() - 1] == '\n' || s[s.length() - 1] == '\r'))
				s.resize(s.length() - 1);
			return s;
		}
	};

	std::string answer;
	int x = 0, y = 0;
	uint freezeMs = 0;
	if(sscanf(line, "hit %d %d", &x, &y) == 2) answer = hitAt(x, y);
	else if(!strncmp(line, "resetstats", 10)) { resetStats(); answer = "ok\n"; }
	else if(sscanf(line, "freeze fade %u", &freezeMs) == 1)
	{
		freezeAtFade(freezeMs);
		answer = "ok\n";
	}
	else if(sscanf(line, "freeze %u", &freezeMs) == 1)
	{
		freezeAt(freezeMs);
		answer = "ok\n";
	}
	else if(sscanf(line, "lockstep %d", &x) == 1) { setLockstep(x != 0); answer = "ok\n"; }
	else if(!strncmp(line, "state ", 6))
	{
		// Switch to a named game state, applied at the loop's safe point like
		// any other change. It is how the harness reaches the credits, which
		// the menu offers only to a held Shift+C.
		Engine::inst().setGameState(Argument::of(line + 6));
		answer = "ok\n";
	}
	else if(!strncmp(line, "click ", 6))
	{
		// Press a named button from inside the game, clock frozen or not -
		// which is the one thing a real click cannot be: one that lands on a
		// named tick. A screen entered this way starts from a picture the
		// tick decides, and not the harness's timing; the star scene leaves
		// the menu like this.
		GUI_Element* p_element = GUI::inst()[Argument::of(line + 6)];
		if(p_element && p_element->getType() == "GUI_Button")
		{
			static_cast<GUI_Button*>(p_element)->click();
			answer = "ok\n";
		}
		else answer = p_element ? "not a button\n" : "no such element\n";
	}
	else if(!strncmp(line, "particles", 9))
	{
		// Every live particle of the played level's two systems, one per
		// line: which system, position, ticks left, size, colour. The
		// diagnostic behind the frame oracle: two frames that differ in a
		// cloud say that something differed, and this says which particle
		// and, from its lifetime, when.
		GameState* p_state = Engine::inst().getGameState();
		Level* p_level = (p_state && p_state->getName() == "GS_Game")
						 ? static_cast<GS_Game*>(p_state)->getLevel() : 0;
		ParticleSystem* systems[2] = {p_level ? p_level->getParticleSystem() : 0,
									  p_level ? p_level->getFireParticleSystem() : 0};
		for(int s = 0; s < 2; s++)
		{
			if(!systems[s]) continue;
			for(ParticleSystem::ParticleList::iterator i = systems[s]->begin(); i != systems[s]->end(); ++i)
			{
				char buffer[160];
				sprintf(buffer, "%d %.3f %.3f %u %.4f %.3f %.3f %.3f %.3f\n", s,
						i->position.x, i->position.y, static_cast<unsigned int>(i->lifetime),
						i->size, i->color.r, i->color.g, i->color.b, i->color.a);
				answer += buffer;
			}
		}
		if(answer.empty()) answer = "none\n";
	}
	else if(!strncmp(line, "shot ", 5))
	{
		// The picture the game itself read out of its framebuffer, at
		// 640x480 whatever the window is doing - so the oracle compares the
		// game's own frame and not a screen grab of a scaled window.
		answer = Engine::inst().writeScreenshot(Argument::of(line + 5)) ? "ok\n" : "failed\n";
	}
	else answer = dump();

	if(!serial.empty()) answer = serial + "\n" + answer;

	const std::string temporaryPath(directory + "/response.tmp");
	FILE* p_response = fopen(temporaryPath.c_str(), "wb");
	if(!p_response) return;
	fwrite(answer.data(), 1, answer.length(), p_response);
	fclose(p_response);
	::rename(temporaryPath.c_str(), (directory + "/response").c_str());
}

#endif // !__EMSCRIPTEN__

}

#ifndef __EMSCRIPTEN__

// What feeds drawCalls. The hooks build links with
// --wrap=glBegin,--wrap=glDrawArrays,--wrap=glDrawElements (LinuxBuild/
// build.sh), which sends every call to those three from the game's own
// objects here and leaves the real entry point under its __real_ name. At
// the link rather than through a macro, so no header carries the define and
// no other translation unit needs it - and the shipped build has none of
// this. The three are the whole of what draws in this tree - the renderer
// with glDrawElements, the present with the other two - since verify.py's
// raw_gl check keeps every gl* call to the files that own raw GL, and
// nothing draws for the game from inside a library, where a wrap could not
// see it.
//
// Not in the browser, where perf.js counts the draws on the WebGL context
// itself - the same number, since nothing stands between the game and WebGL
// there.
extern "C"
{
	void __real_glBegin(GLenum mode);
	void __real_glDrawArrays(GLenum mode, GLint first, GLsizei count);
	void __real_glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid* p_indices);

	void __wrap_glBegin(GLenum mode)
	{
		TestHooks::drawCalls++;
		__real_glBegin(mode);
	}

	void __wrap_glDrawArrays(GLenum mode, GLint first, GLsizei count)
	{
		TestHooks::drawCalls++;
		__real_glDrawArrays(mode, first, count);
	}

	void __wrap_glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid* p_indices)
	{
		TestHooks::drawCalls++;
		__real_glDrawElements(mode, count, type, p_indices);
	}
}

#endif // !__EMSCRIPTEN__

#endif // BLOCKS5_TEST_HOOKS
