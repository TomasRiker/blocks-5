#include "pch.h"
#ifdef __EMSCRIPTEN__
#include "web_transfer.h"
#include "filesystem.h"
#include <emscripten.h>

// Achtung: EM_ASM-Rumpfe laufen durch den C-Praeprozessor. Ein einzelnes
// Apostroph (auch '' ) ist dort ein kaputtes Zeichenliteral - im JS also
// ausschliesslich doppelte Anfuehrungszeichen benutzen.

namespace
{
	char importName[260] = "";
	volatile int importStatus = WebTransfer::IMPORT_IDLE;
	bool busy = false;
}

extern "C" {
EMSCRIPTEN_KEEPALIVE char* blocks5_importNameBuffer(void)   { return importName; }
EMSCRIPTEN_KEEPALIVE int   blocks5_importNameCapacity(void) { return (int)sizeof(importName); }
EMSCRIPTEN_KEEPALIVE void  blocks5_importComplete(int status) { importStatus = status; }
}

namespace WebTransfer
{

void download(const std::string& vfsPath, const std::string& downloadName)
{
	EM_ASM({
		var path = UTF8ToString($0);
		var name = UTF8ToString($1);
		try {
			// FS.readFile liefert ein frisches Uint8Array (libfs.js:1431),
			// also keinen Blick in den wasm-Heap - unter ALLOW_MEMORY_GROWTH
			// gefahrlos an einen Blob weiterzureichen. Kein Text-Encoding:
			// Level-XML ist ISO-8859-1 und muss es bleiben.
			var url = URL.createObjectURL(new Blob([FS.readFile(path)],
			                              { type: "application/octet-stream" }));
			var a = document.createElement("a");
			a.href = url;
			a.download = name;
			a.style.display = "none";
			document.body.appendChild(a);
			a.click();
			a.remove();
			// Sofortiges revoke bricht den Download in manchen Browsern ab.
			setTimeout(function() { URL.revokeObjectURL(url); }, 60000);
		} catch (e) { console.warn("[blocks5] export failed:", e); }
	}, vfsPath.c_str(), downloadName.c_str());
}

bool openPicker(const std::string& stagingOgg,
                const std::string& stagingXml,
                const std::string& stagingZip,
                unsigned int maxBytes)
{
	if(busy) return false;

	// Der Klick kommt aus der SDL-Ereignisschlange, also nicht mehr aus dem
	// DOM-Handler (libsdl.js:1455). Der Dateidialog braucht aber eine gueltige
	// Benutzer-Aktivierung; die haelt der Browser ~5 s vor, was fuer den einen
	// Frame Verzoegerung reicht. Wenn sie erkennbar abgelaufen ist, lieber
	// ehrlich "nochmal klicken" sagen als einen stillen Fehlschlag liefern.
	if(EM_ASM_INT({
		return (navigator.userActivation && !navigator.userActivation.isActive) ? 1 : 0;
	})) return false;

	// Reste eines fehlgeschlagenen Versuchs wegraeumen, sonst koennte eine
	// spaetere Pruefung die alten Bytes sehen.
	FileSystem::inst().deleteFile(stagingOgg);
	FileSystem::inst().deleteFile(stagingXml);
	FileSystem::inst().deleteFile(stagingZip);

	busy = true;
	importStatus = IMPORT_IDLE;
	importName[0] = 0;

	EM_ASM({
		// Kein Objektliteral: der Rumpf laeuft durch den C-Praeprozessor, und
		// jedes Komma ausserhalb von Klammern teilt dort die Makro-Argumente.
		var paths = {};
		paths[".ogg"] = UTF8ToString($0);
		paths[".xml"] = UTF8ToString($1);
		paths[".zip"] = UTF8ToString($2);
		var maxSize = $3 >>> 0;
		var finished = false;

		function done(status, name) {
			if (finished) return;
			finished = true;
			if (name) {
				// Nur ein Vorschlag - C bildet den Zielpfad selbst.
				var p   = Module["_blocks5_importNameBuffer"]();
				var cap = Module["_blocks5_importNameCapacity"]();
				var n   = Math.min(name.length, cap - 1);
				// HEAPU8 hier frisch lesen: die Referenz wird bei
				// Speicherwachstum ersetzt.
				for (var i = 0; i < n; i++) {
					var c = name.charCodeAt(i);
					HEAPU8[p + i] = (c >= 32 && c < 127) ? c : 95;
				}
				HEAPU8[p + n] = 0;
			}
			Module["_blocks5_importComplete"](status);
			if (input.parentNode) input.remove();
		}

		var input = document.createElement("input");
		input.type = "file";
		input.accept = ".ogg,.xml,.zip";
		input.style.display = "none";
		input.addEventListener("cancel", function() { done(2, null); });
		input.addEventListener("change", function() {
			var f = input.files && input.files[0];
			if (!f) { done(2, null); return; }
			var dot = f.name.lastIndexOf(".");
			var ext = (dot < 0) ? "" : f.name.substring(dot).toLowerCase();
			var staging = paths[ext];
			if (!staging)         { done(4, f.name); return; }
			if (f.size > maxSize) { done(3, f.name); return; }
			var r = new FileReader();
			r.onerror = function() { done(5, f.name); };
			r.onload = function() {
				try {
					FS.writeFile(staging, new Uint8Array(r.result));
					done(1, f.name);
				} catch (e) {
					console.warn("[blocks5] import failed:", e);
					done(5, f.name);
				}
			};
			r.readAsArrayBuffer(f);
		});
		document.body.appendChild(input);
		input.click();
		// Browser ohne "cancel"-Ereignis wuerden den Knopf sonst ewig
		// belegen. Das Spiel blockiert nie - es fragt nur ab.
		setTimeout(function() { if (!finished) done(2, null); }, 300000);
	}, stagingOgg.c_str(), stagingXml.c_str(), stagingZip.c_str(), (int)maxBytes);

	return true;
}

int pollImport(std::string& untrustedName)
{
	const int status = importStatus;
	if(status == IMPORT_IDLE) return IMPORT_IDLE;
	importStatus = IMPORT_IDLE;
	busy = false;
	untrustedName = importName;
	return status;
}

void abandon()
{
	importStatus = IMPORT_IDLE;
	busy = false;
	importName[0] = 0;
}

void syncHome()
{
	EM_ASM({ if (Module["b5_sync"]) Module["b5_sync"](); });
}

}
#endif