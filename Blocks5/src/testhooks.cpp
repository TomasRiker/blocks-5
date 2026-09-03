#include "pch.h"
#include "testhooks.h"

// testhooks.cpp - der Bericht ueber den GUI-Baum, den beide Testwege lesen.
//
// Was hier steht, ist plattformunabhaengig: es liest Engine und GUI aus und
// baut daraus JSON. Wie der Text nach draussen kommt, steht woanders - im
// Browser in WebBuild/test_hooks.cpp ueber Module["b5_test"], unter Linux
// unten in pollRequests() ueber eine Datei.

#ifdef BLOCKS5_TEST_HOOKS

#include "engine.h"
#include "gamestate.h"
#include "gui.h"
#include "gui_element.h"

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

	// Spielkoordinate -> Fensterkoordinate, dieselbe Rechnung wie in
	// presentFrame(). Die Umkehrung davon macht Engine::getCursorPosition(),
	// ein Klick auf den gelieferten Punkt landet also genau hier.
	//
	// Die Woelbung des Roehrenfilters bleibt aussen vor: sie kaeme aus
	// warpToOutput(), das privat ist, und ein Test, der Knoepfe trifft, laeuft
	// ohnehin ohne CRT-Filter. canUseCrt() steht mit im JSON, damit ein Test
	// bemerkt, wenn dieser Fall doch eintritt.
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

		// Gefragt werden kann frueher, als man denkt - im Browser steht die
		// Ausfuhr schon bereit, sobald das Modul geladen ist, also lange vor
		// main(). Vorher steht in screenSize eine Null, computePresentRect()
		// teilt dadurch, und die Umwandlung des entstehenden NaN nach int ist
		// in wasm kein falscher Wert, sondern ein Trap: die Seite haengt,
		// statt einen Fehler zu melden. Also erst nachsehen, ob es ueberhaupt
		// schon etwas zu berichten gibt.
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
		appendEscaped(out, Engine::getUpscaleFilterName(engine.getEffectiveUpscaleFilter()));
		out += "\",\"crt\":";
		out += (engine.getEffectiveUpscaleFilter() == Engine::UF_CRT) ? "true" : "false";
		out += ",\"focus\":\"";
		appendEscaped(out, p_focus ? p_focus->getFullName() : "");
		// Wo das Spiel den Zeiger sieht und worauf er gerade drueckt. Mit
		// einer Maus ist das nie eine Frage - man kann nicht dorthin
		// klicken, wo der Zeiger nicht ist. Ein Finger aber setzt auf, ohne
		// sich vorher bewegt zu haben, und ein Tippen, das nicht ankommt,
		// sieht von aussen genauso aus wie ein Knopf, der nicht reagiert.
		out += "\",\"mouseDown\":\"";
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

		// Ab den Kindern der Wurzel: die Wurzel selbst hat keinen Namen, unter
		// dem sie jemand suchen wuerde.
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

std::string hitAt(int x, int y)
{
	GUI_Element* p_root = GUI::inst().getRoot();
	GUI_Element* p_hit = p_root ? p_root->getElementAt(Vec2i(x, y)) : 0;
	return p_hit ? p_hit->getFullName() : std::string();
}

#ifndef __EMSCRIPTEN__

// Im Browser ruft JavaScript die Ausfuhr auf. Nativ gibt es keinen solchen
// Draht, also liegt die Anfrage in einer Datei: der Test schreibt "request",
// das Spiel liest sie einmal je Logiktakt, loescht sie und legt die Antwort
// daneben. Geschrieben wird erst unter einem anderen Namen und dann umbenannt,
// weil das ein einziger Schritt ist - der Test sieht die Antwort nie halb
// fertig.
//
// Ein stat() auf eine Datei, die es nicht gibt, fuenfzigmal in der Sekunde,
// kostet nichts, und den Weg gibt es nur im Testbuild.
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
