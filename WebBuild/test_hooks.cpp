#include "pch.h"

// test_hooks.cpp - eine Auskunftsstelle fuer die Browsersteuerung beim Testen.
//
// Der Web-Build laesst sich fernsteuern (Playwright, siehe test/README.md), aber
// die Oberflaeche kennt nur Pixel: wer einen Knopf treffen will, muss seine
// Bildschirmkoordinate raten und aus einem Bildschirmfoto ablesen. Das geht
// regelmaessig daneben - der Knopf ist 18 Pixel hoch, das Fenster ist skaliert,
// und ob wirklich er getroffen wurde oder das Element darunter, sieht man dem
// Foto nicht an.
//
// Diese Datei beantwortet das statt dessen: sie legt den GUI-Baum mit den
// Fensterkoordinaten jedes Elements als JSON in Module["b5_test"]. Der Test
// klickt dann auf einen Namen ("Menu.Options") und nicht auf eine Zahl.
//
// Sie liest nur und ruft nichts auf, was den Zustand aendert - die Eingabe
// selbst kommt weiterhin als echter Mausklick vom Browser, damit der Weg
// durch SDL, Engine und GUI derselbe bleibt wie im Spiel.
//
// Gebaut wird sie nur mit ./build.sh hooks; ohne -DBLOCKS5_TEST_HOOKS ist die
// Uebersetzungseinheit leer, und der ausgelieferte Build enthaelt nichts davon.

#if defined(__EMSCRIPTEN__) && defined(BLOCKS5_TEST_HOOKS)

#include "engine.h"
#include "gamestate.h"
#include "gui.h"
#include "gui_element.h"
#include <emscripten.h>

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

		// Der Browser hat die Ausfuhr, sobald das Modul geladen ist - also
		// laenger vor main(), als man denkt. Vorher steht in screenSize eine
		// Null, computePresentRect() teilt dadurch, und die Umwandlung des
		// entstehenden NaN nach int ist in wasm kein falscher Wert, sondern
		// ein Trap: die Seite haengt, statt einen Fehler zu melden. Also erst
		// nachsehen, ob es ueberhaupt schon etwas zu berichten gibt.
		if(screen.x <= 0 || screen.y <= 0 || !GUI::inst().getRoot())
		{
			return std::string("{\"state\":\"\",\"elements\":[]}\n");
		}

		int px, py, pw, ph;
		engine.computePresentRect(px, py, pw, ph);

		GameState* p_state = engine.getGameState();
		GUI_Element* p_focus = GUI::inst().getFocusElement();

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
		out += "\",";
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

extern "C"
{
	// Legt den Bericht in Module["b5_test"] ab. Der Rueckweg ueber eine
	// C-Zeichenkette braeuchte ccall/UTF8ToString unter den exportierten
	// Laufzeitmethoden; im EM_ASM-Rumpf ist UTF8ToString ohnehin da.
	EMSCRIPTEN_KEEPALIVE void blocks5_testDump(void)
	{
		const std::string json = buildDump();
		EM_ASM({ Module["b5_test"] = UTF8ToString($0); }, json.c_str());
	}

	// Wer bekaeme einen Klick auf diesen Punkt? getElementAt() geht denselben
	// Weg wie GUI::update() und beantwortet damit die Frage, an der ein Test
	// sonst scheitert: liegt etwas anderes darueber?
	//
	// Einzeln und nicht als Feld im Bericht: containsPoint() misst bei einem
	// Schalter die Breite seiner Beschriftung, und das je Element fuer jedes
	// Element waeren bei zweihundert Elementen vierzigtausend Messungen.
	EMSCRIPTEN_KEEPALIVE void blocks5_testHitAt(int x, int y)
	{
		GUI_Element* p_root = GUI::inst().getRoot();
		GUI_Element* p_hit = p_root ? p_root->getElementAt(Vec2i(x, y)) : 0;
		const std::string name(p_hit ? p_hit->getFullName() : "");
		EM_ASM({ Module["b5_hit"] = UTF8ToString($0); }, name.c_str());
	}
}

#endif
