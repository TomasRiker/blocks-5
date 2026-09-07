#include "pch.h"
#ifdef __EMSCRIPTEN__
#include "web_transfer.h"
#include "filesystem.h"
#include <emscripten.h>

// EM_ASM bodies run through the C preprocessor, where a single apostrophe
// (even '' ) is a broken character literal - use nothing but double quotes
// in the JS.

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
			// FS.readFile returns a fresh Uint8Array (libfs.js:1431), not
			// a view into the wasm heap - safe to hand to a Blob under
			// ALLOW_MEMORY_GROWTH. No text encoding: level XML is
			// ISO-8859-1 and must stay that way.
			var url = URL.createObjectURL(new Blob([FS.readFile(path)],
			                              { type: "application/octet-stream" }));
			var a = document.createElement("a");
			a.href = url;
			a.download = name;
			a.style.display = "none";
			document.body.appendChild(a);
			a.click();
			a.remove();
			// Revoking at once aborts the download in some browsers.
			setTimeout(function() { URL.revokeObjectURL(url); }, 60000);
		} catch (e) { console.warn("[blocks5] export failed:", e); }
	}, vfsPath.c_str(), downloadName.c_str());
}

void downloadBytes(const void* p_data, unsigned int numBytes,
                   const std::string& downloadName)
{
	EM_ASM({
		var name = UTF8ToString($2);
		try {
			// subarray returns a view into the wasm heap that a growing
			// memory invalidates at any time - the Uint8Array constructor
			// copies it here and now, and the Blob gets the copy.
			var bytes = new Uint8Array(HEAPU8.subarray($0, $0 + $1));
			var url = URL.createObjectURL(new Blob([bytes],
			                              { type: "image/png" }));
			var a = document.createElement("a");
			a.href = url;
			a.download = name;
			a.style.display = "none";
			document.body.appendChild(a);
			a.click();
			a.remove();
			// Revoking at once aborts the download in some browsers.
			setTimeout(function() { URL.revokeObjectURL(url); }, 60000);
		} catch (e) { console.warn("[blocks5] screenshot failed:", e); }
	}, p_data, numBytes, downloadName.c_str());
}

bool openPicker(const std::string& stagingOgg,
                const std::string& stagingXml,
                const std::string& stagingZip,
                unsigned int maxBytes)
{
	if(busy) return false;

	// The click comes out of the SDL event queue and therefore no longer out
	// of the DOM handler (libsdl.js:1455). The file dialog needs a valid user
	// activation, though; the browser keeps one for ~5 s, which covers that
	// one frame of delay. Where it has plainly expired, say "click again"
	// honestly rather than failing silently.
	if(EM_ASM_INT({
		return (navigator.userActivation && !navigator.userActivation.isActive) ? 1 : 0;
	})) return false;

	// Clear away what a failed attempt left behind, or a later check could
	// see the old bytes.
	FileSystem::inst().deleteFile(stagingOgg);
	FileSystem::inst().deleteFile(stagingXml);
	FileSystem::inst().deleteFile(stagingZip);

	busy = true;
	importStatus = IMPORT_IDLE;
	importName[0] = 0;

	EM_ASM({
		// No object literal: the body runs through the C preprocessor, where
		// every comma outside parentheses splits the macro arguments.
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
				// Only a suggestion - C composes the target path itself.
				var p   = Module["_blocks5_importNameBuffer"]();
				var cap = Module["_blocks5_importNameCapacity"]();
				var n   = Math.min(name.length, cap - 1);
				// Read HEAPU8 freshly here: the reference is replaced on
				// memory growth.
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
		// Browsers without a "cancel" event would otherwise tie the button up
		// for ever. The game never blocks - it only polls.
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