#include "pch.h"
#include "web_bluescreen.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>

namespace
{
	// 80 Spalten, wie das Original. Reiner ASCII-Text, damit die Datei in jeder
	// Kodierung dasselbe bedeutet.
	const char* p_text =
		"A problem has been detected and Blocks has been shut down to prevent damage\n"
		"to your computer.\n"
		"\n"
		"The problem seems to be caused by the following file: BLOCKS5.SYS\n"
		"\n"
		"TOO_MUCH_FUN_IN_NONPAGED_AREA\n"
		"\n"
		"If this is the first time you've seen this Stop error screen,\n"
		"restart your computer. If this screen appears again, follow\n"
		"these steps:\n"
		"\n"
		"Check to make sure any new dynamite is properly installed. If this is a\n"
		"new installation, ask your level designer for any updates you might need.\n"
		"\n"
		"If problems continue, disable or remove any newly planted bombs. Disable\n"
		"BIOS memory options such as caching or shadowing. If you need to use Safe\n"
		"Mode to remove or disable components, restart your computer, press F8 to\n"
		"select Advanced Startup Options, and then select Safe Mode.\n"
		"\n"
		"Technical information:\n"
		"\n"
		"*** STOP: 0x0000B10C (0xB0B5EA75,0x00000001,0xDEADD1CE,0x00000000)\n"
		"\n"
		"\n"
		"***  BLOCKS5.SYS - Address DEADD1CE base at B10C5000, DateStamp 3d6dd67c\n"
		"\n"
		"Beginning dump of physical memory\n"
		"Physical memory dump complete.\n"
		"Contact your level designer or technical support group for further\n"
		"assistance.\n";
}

void WebBlueScreen::show()
{
	// Erst still werden. Die Hauptschleife steht gleich, und ohne
	// updateSounds() liefe die Musik sonst noch eine Pufferlaenge weiter.
	alListenerf(AL_GAIN, 0.0f);

	EM_ASM({
		if(document.getElementById('blocks5-bsod')) return;

		// Aus dem Vollbild heraus, sonst laege die Einblendung dahinter.
		if(document.fullscreenElement && document.exitFullscreen) {
			try { document.exitFullscreen(); } catch(e) {}
		}

		var pre = document.createElement('pre');
		pre.textContent = UTF8ToString($0);
		pre.style.cssText = 'margin:0;font:inherit;white-space:pre;';

		var hint = document.createElement('div');
		hint.textContent = 'Press any key to restart your computer ...';
		hint.style.cssText = 'margin-top:2em;text-align:center;';

		var box = document.createElement('div');
		box.appendChild(pre);
		box.appendChild(hint);
		// 80 Spalten sollen hineinpassen, ohne dass es auf einem grossen Schirm
		// albern gross wird: 0.6em ist ungefaehr die Zeichenbreite einer
		// Schreibmaschinenschrift, also 80 * 0.6 = 48em Textbreite.
		box.style.cssText =
			'font-family:"Lucida Console",Consolas,"Courier New",monospace;' +
			'font-size:clamp(7px,min(1.55vw,2.6vh),19px);line-height:1.35;' +
			'color:#fff;max-width:52em;';

		var screen = document.createElement('div');
		screen.id = 'blocks5-bsod';
		screen.appendChild(box);
		screen.style.cssText =
			'position:fixed;left:0;top:0;width:100%;height:100%;z-index:2147483647;' +
			'background:#0000aa;display:flex;align-items:center;justify-content:center;' +
			'cursor:none;overflow:hidden;';
		document.body.appendChild(screen);

		// Neustarten heisst hier: die Seite neu laden. Kurz gesperrt, damit der
		// Klick, der das hier ausgeloest hat, ihn nicht sofort wieder wegnimmt.
		var armed = false;
		setTimeout(function(){ armed = true; }, 700);
		var restart = function(){ if(armed) location.reload(); };
		window.addEventListener('keydown', restart);
		window.addEventListener('mousedown', restart);
		window.addEventListener('touchstart', restart);
	}, p_text);

	// Die Beruehrungsabkuerzung ins Vollbild muss hier weg. Sonst holt genau
	// der Fingertipp, der neu laden soll, den Bildschirm vorher noch einmal ins
	// Vollbild - und die Einblendung laege wieder hinter dem Canvas, den sie
	// gerade verdecken soll.
	emscripten_set_touchstart_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, 0, EM_TRUE, 0);

	emscripten_cancel_main_loop();
}

#else
void WebBlueScreen::show() {}
#endif
