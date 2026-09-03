// sw.js - the offline cache for the browser build.
//
// The three payload files belong together. blocks5-<build>.js carries a table
// of absolute byte offsets into blocks5-<build>.data, and its EM_ASM fragments
// sit at addresses that fit exactly one blocks5-<build>.wasm. Handed out in
// mismatched pairs the game aborts - "No EM_ASM constant found at address ..."
// is what that looks like from the console.
//
// The build's stamp in those three filenames is what makes that impossible:
// what lies under such a URL never changes, so no cache on the way - this one,
// the browser's own, a proxy, mod_pagespeed - can serve one build's JavaScript
// beside another build's wasm. It is also why the two halves below are cached
// in opposite directions:
//
//   stamped payload   cache first. It is immutable, so asking the network
//                     could only ever confirm what is already here.
//   everything else   network first, cache as the fallback. index.html cannot
//                     carry a stamp - it is the entry point, and it is where
//                     the current stamp is written down - so serving it from
//                     the cache would mean nobody ever learns that a new build
//                     exists. Without a network it still comes from the cache.
//
// skipWaiting and clients.claim are safe for the same reason: a page that has
// booted holds stamped URLs, so a worker taking over behind it cannot hand it
// a different build halfway through.
//
// %%VERSION%% is replaced by build.sh with a hash of the three files, so the
// cache name changes exactly when the payload does and never otherwise.
var BUILD = '%%VERSION%%';
var CACHE = 'blocks5-' + BUILD;

// Without these the game cannot start, and a half-cached set is worse than
// none: they are fetched as one unit.
var PAYLOAD = ['./index.html',
               './blocks5-' + BUILD + '.js',
               './blocks5-' + BUILD + '.wasm',
               './blocks5-' + BUILD + '.data'];

// Nice to have. './' is what a home-screen launch asks for, but a server that
// does not serve a directory index would fail the whole install over it.
var EXTRA = ['./', './blocks5.html', './manifest.json',
             './icon-192.png', './icon-512.png', './icon-maskable-512.png',
             './apple-touch-icon.png'];

// Gestempelt und damit unveraenderlich - und davon die drei dieses Baus.
var STAMPED = /\/blocks5-[0-9a-f]+\.(js|wasm|data)$/;
var MINE = new RegExp('/blocks5-' + BUILD + '\\.(js|wasm|data)$');

self.addEventListener('install', function (e) {
	self.skipWaiting();
	e.waitUntil(caches.open(CACHE).then(function (c) {
		return c.addAll(PAYLOAD).then(function () {
			return Promise.all(EXTRA.map(function (u) { return c.add(u).catch(function () {}); }));
		});
	}));
});

self.addEventListener('activate', function (e) {
	e.waitUntil(caches.keys().then(function (names) {
		return Promise.all(names.map(function (n) {
			return n === CACHE ? null : caches.delete(n);
		}));
	}).then(function () { return self.clients.claim(); }));
});

self.addEventListener('fetch', function (e) {
	var req = e.request;
	if (req.method !== 'GET') return;
	var url = new URL(req.url);
	if (url.origin !== self.location.origin) return;

	// Only a plain, successful, same-origin answer is worth keeping; an opaque
	// or partial one would poison the cache.
	function keep(c, res) {
		if (res && res.status === 200 && res.type === 'basic') c.put(req, res.clone());
		return res;
	}

	if (STAMPED.test(url.pathname)) {
		// Nur die Nutzlast des eigenen Baus gehoert in diesen Speicher. Waehrend
		// ein neuer Worker installiert, bedient der alte die Seite noch, und ohne
		// diese Zeile legte er die Dateien des neuen Baus in seinen eigenen, gleich
		// zu loeschenden Speicher - kurzzeitig beide Nutzlasten auf der Platte.
		// Fremd gestempeltes laesst er stattdessen ganz durch.
		if (!MINE.test(url.pathname)) return;

		e.respondWith(caches.open(CACHE).then(function (c) {
			return c.match(req, { ignoreSearch: true }).then(function (hit) {
				return hit || fetch(req).then(function (res) { return keep(c, res); });
			});
		}));
		return;
	}

	e.respondWith(caches.open(CACHE).then(function (c) {
		return fetch(req).then(function (res) {
			return keep(c, res);
		}).catch(function () {
			// Kein Netz: dann das, was beim Installieren hereingekommen ist.
			return c.match(req, { ignoreSearch: true }).then(function (hit) {
				return hit || c.match('./index.html');
			});
		});
	}));
});
