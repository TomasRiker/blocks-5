#include "pch.h"
#include "testhooks.h"

// testhooks.cpp - the report on the GUI tree that both test paths read, built
// as JSON from Engine and GUI. In the browser it leaves through
// Module["b5_test"] (WebBuild/test_hooks.cpp), natively through a file in
// pollRequests() at the bottom.

#ifdef BLOCKS5_TEST_HOOKS

#include "engine.h"
#include "upscaler.h"
#include "gamestate.h"
#include "gs_game.h"
#include "level.h"
#include "gui.h"
#include "gui_element.h"
#include "gui_button.h"
#include "gui_statictext.h"
#include "player.h"
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
	void appendFixed(std::string& out, float value)
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
	// The CRT filter's curvature is left out, since a test that hits buttons
	// runs without it; "crt" in the JSON says when something warps after all.
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

		// A static text's laid-out size, so a harness can ask whether it still
		// fits its box. Measured through the element's own font and wrap, the
		// only answer that follows a language switch and a rebound key.
		GUI_StaticText* p_staticText = dynamic_cast<GUI_StaticText*>(p_element);
		if(p_staticText)
		{
			const Vec2i dim = p_staticText->measureDrawnText();
			out += ",";
			appendPoint(out, "text", dim.x, dim.y);
		}

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

		// In the browser the export can be called before main() has run.
		// screenSize is then zero, computePresentRect() divides by it, and in
		// wasm the cast of the resulting NaN to int traps, which hangs the page
		// instead of reporting an error.
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
		// Whether anything warps the picture, not whether the CRT filter is on:
		// only then are the window coordinates wrong. The key is named crt
		// because WebBuild/test/harness.js reads it under that name.
		out += "\",\"crt\":";
		out += engine.getUpscaler()->distortsCursor() ? "true" : "false";
		out += ",\"focus\":\"";
		appendEscaped(out, p_focus ? p_focus->getFullName() : "");
		out += "\",\"appActive\":";
		out += engine.isAppActive() ? "true" : "false";

		// Only the game state can be paused. It is recognised by name, the way
		// game states are told apart throughout the tree.
		out += ",\"paused\":";
		out += (p_state && p_state->getName() == "GS_Game"
				&& static_cast<GS_Game*>(p_state)->isPaused()) ? "true" : "false";

		// The named actions that are down, and only those. This is the one
		// view of the action layer from outside: it reads SDL_GetKeyState and
		// not keyData, so nothing else shows whether a key reached it.
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

		// The most particles one system has had to draw since the last dump,
		// which is what sizes ParticleSystem's vertex buffer. Reading it clears
		// it, so a dump at the start and one at the end of a scene give the
		// peak over exactly that stretch.
		out += ",\"particlePeak\":";
		appendInt(out, static_cast<int>(ParticleSystem::takePeakCount()));

		// time is the engine's logic clock and scene the scene's own
		// (Engine::sceneTick), both in milliseconds. The seeded generator is
		// keyed on scene, so a test that wants a named frame waits for that
		// rather than for an interval.
		out += ",\"time\":";
		appendInt(out, static_cast<int>(engine.getTime()));
		out += ",\"scene\":";
		appendInt(out, static_cast<int>(engine.sceneTick));
		out += ",\"frozen\":";
		out += TestHooks::frozen() ? "true" : "false";

		// How far a running transition has got in milliseconds, or -1 where
		// there is none: the number "freeze fade" stops on, which lets a
		// harness measure a transition's length.
		out += ",\"crossfade\":";
		appendInt(out, engine.getCrossfadeProgressMs());

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
		// Every draw call render() made, over the frames it made them in: the
		// number a renderer change is measured by. calls comes from the
		// wrappers at the foot of this file; in the browser, where the harness
		// counts on the WebGL context, it is left out rather than reported as
		// a zero that would read as frames drawing nothing.
		out += ",\"draws\":{\"frames\":";
		appendInt(out, static_cast<int>(engine.renderedFrames));
#ifndef __EMSCRIPTEN__
		out += ",\"calls\":";
		appendInt(out, static_cast<int>(engine.renderDraws));
#endif
		out += "}";

		// What the font caches did over the same frames. The four sizes
		// (entries, quads, dimEntries, dimBytes) are what the caches hold now,
		// not counts since the reset; quads is the unit the budget is spent
		// in, 64 bytes of glyph geometry per character. Of the measures (calls
		// to measureText()), measureHits were answered by a laid-out string
		// and dimHits by the dimensions cache; the rest walked the string.
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

		// What the frames since the last resetStats() cost, in milliseconds
		// on the main thread. In the browser none of it waits for the GPU
		// (WebGL takes a command and returns), so there these are what the
		// game and the JavaScript cost, the half that starves the audio and
		// drops frames. Natively the GPU's share lands in present and swap.
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
			// Counted against one logic tick: late is the frames whose
			// interval went over it, lateWork those whose own work did. The
			// two come apart - a browser at 38 ms a frame on 2.7 ms of work has
			// all of the first and none of the second. over500 is Emscripten's
			// Web Audio lookahead, so a frame past it is a hole in the music.
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

		// The cell the active character stands on, or -1,-1 with no level
		// running or nobody awake. A drag steers the field and not a widget,
		// so this is how a test sees whether a gesture walked anybody.
		{
			GameState* p_game = engine.getGameState();
			Level* p_lvl = (p_game && p_game->getName() == "GS_Game")
						   ? static_cast<GS_Game*>(p_game)->getLevel() : 0;
			Player* p_player = p_lvl ? p_lvl->getActivePlayer() : 0;
			const Vec2i cell = p_player ? p_player->getPosition() : Vec2i(-1, -1);
			out += ",";
			appendPoint(out, "player", cell.x, cell.y);

			// Whether the lights are out, for the same reason: of what a switch
			// can do, this is the effect that is a single bit of the level
			// rather than something to recognise in a picture.
			out += ",\"nightVision\":";
			out += (p_lvl && p_lvl->isNightVision()) ? "true" : "false";
		}

		// What the game is pressing on and where it sees the cursor: without
		// them a tap that never arrives looks exactly like a button that does
		// not react.
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

// Natively there is no JavaScript to call the export, so the request comes
// through a file. The test publishes "request" by renaming it into place,
// since a file created and then written can be read empty in between; the
// game reads it once per logic tick, deletes it and answers in "response",
// written as response.tmp and renamed so that the test never sees half an
// answer. An fopen() of a missing file fifty times a second costs nothing.
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

	// One line per request. A shot's path alone can run past a hundred
	// characters, and fgets cuts a longer line short without a word, so the
	// buffer is generous.
	char buffer[1024] = "";
	if(!fgets(buffer, sizeof(buffer), p_request)) buffer[0] = 0;
	fclose(p_request);
	::remove(requestPath.c_str());

	// "#<serial> <request>": the serial goes back on the answer's first line,
	// so the harness can tell this answer from a late one to an earlier
	// request it had given up on.
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
		// any other change. It reaches the credits in the version the harness
		// wants, where the menu would pick it from the player's progress. One
		// optional word after the name is set to true in the context the state
		// is entered with: "state GS_Credits full" asks for the ending.
		const std::string arg(Argument::of(line + 6));
		const std::string::size_type space = arg.find(' ');
		ParameterBlock context;
		if(space != std::string::npos) context.set(arg.substr(space + 1), true);
		Engine::inst().setGameState(arg.substr(0, space), context);
		answer = "ok\n";
	}
	else if(!strncmp(line, "click ", 6))
	{
		// Press a named button from inside the game, frozen clock or not.
		// Unlike a real click it lands on a named tick, so the screen it
		// opens starts from a picture the tick decides; the star and cube
		// scenes start their transitions this way.
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
		// line: system, position, ticks left, size, colour. When two oracle
		// frames differ in a cloud, this says which particle and, from its
		// lifetime, since when.
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
// --wrap=glDrawArrays,--wrap=glDrawElements (LinuxBuild/build.sh), which
// routes every call to those two from the game's own object files here and
// leaves the real entry point as __real_; at the link, so no header carries a
// define. The two are all that draws in this tree (the renderer and the
// present), since verify.py's raw_gl check keeps gl* calls to the files that
// own raw GL and no library draws for the game. In the browser
// WebBuild/test/harness.js counts on the WebGL context instead.
extern "C"
{
	void __real_glDrawArrays(GLenum mode, GLint first, GLsizei count);
	void __real_glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid* p_indices);

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
