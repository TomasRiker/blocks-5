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
	char g_importName[260] = "";
	volatile int g_importStatus = WebTransfer::IMPORT_IDLE;
	bool g_busy = false;
	int  g_channel = 0;
}

extern "C" {
EMSCRIPTEN_KEEPALIVE char* blocks5_importNameBuffer(void)   { return g_importName; }
EMSCRIPTEN_KEEPALIVE int   blocks5_importNameCapacity(void) { return (int)sizeof(g_importName); }
EMSCRIPTEN_KEEPALIVE void  blocks5_importComplete(int status) { g_importStatus = status; }
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

bool downloadString(const std::string& data, const std::string& downloadName)
{
	const std::string tmp("/blocks5_export.tmp");   // MEMFS, nicht IDBFS
	if(!FileSystem::inst().writeStringToFile(data, tmp)) return false;
	download(tmp, downloadName);                     // EM_ASM ist synchron
	FileSystem::inst().deleteFile(tmp);
	return true;
}

bool openPicker(int channel,
                const std::string& acceptExtension,
                unsigned int maxBytes,
                const std::string& stagingPath)
{
	if(g_busy) return false;

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
	FileSystem::inst().deleteFile(stagingPath);

	g_busy = true;
	g_channel = channel;
	g_importStatus = IMPORT_IDLE;
	g_importName[0] = 0;

	EM_ASM({
		var accept  = UTF8ToString($0);
		var maxSize = $1 >>> 0;
		var staging = UTF8ToString($2);
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
		input.accept = accept;
		input.style.display = "none";
		input.addEventListener("cancel", function() { done(2, null); });
		input.addEventListener("change", function() {
			var f = input.files && input.files[0];
			if (!f) { done(2, null); return; }
			var dot = f.name.lastIndexOf(".");
			var ext = (dot < 0) ? "" : f.name.substring(dot).toLowerCase();
			if (ext !== accept)     { done(4, f.name); return; }
			if (f.size > maxSize)   { done(3, f.name); return; }
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
	}, acceptExtension.c_str(), (int)maxBytes, stagingPath.c_str());

	return true;
}

int pollImport(int channel, std::string& untrustedName)
{
	if(channel != g_channel) return IMPORT_IDLE;
	const int status = g_importStatus;
	if(status == IMPORT_IDLE) return IMPORT_IDLE;
	g_importStatus = IMPORT_IDLE;
	g_busy = false;
	g_channel = 0;
	untrustedName = g_importName;
	return status;
}

void abandon(int channel, const std::string& stagingPath)
{
	if(channel != g_channel) return;
	g_importStatus = IMPORT_IDLE;
	g_busy = false;
	g_channel = 0;
	g_importName[0] = 0;
	FileSystem::inst().deleteFile(stagingPath);
}

void syncHome()
{
	EM_ASM({ if (Module["b5_sync"]) Module["b5_sync"](); });
}

}
#endif