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
// The pad's own stamp. Separate from BUILD because touch_controls.js is not one
// of the three files BUILD hashes, so the two move independently - which is the
// whole point of giving it a stamp at all.
var PAD = '%%PAD%%';
var CACHE = 'blocks5-' + BUILD;

// Without these the game cannot start, and a half-cached set is worse than
// none: they are fetched as one unit.
var PAYLOAD = ['./index.html',
               './blocks5-' + BUILD + '.js',
               './blocks5-' + BUILD + '.wasm',
               './blocks5-' + BUILD + '.data'];

// Nice to have. './' is what a home-screen launch asks for, but a server that
// does not serve a directory index would fail the whole install over it.
var EXTRA = ['./', './blocks5.html', './manifest.json', './touch_controls-' + PAD + '.js',
             './icon-192.png', './icon-512.png', './icon-maskable-512.png',
             './apple-touch-icon.png'];

// Stamped and therefore immutable - and of those, the ones this build owns.
var STAMPED = /\/(blocks5-[0-9a-f]+\.(js|wasm|data)|touch_controls-[0-9a-f]+\.js)$/;
var MINE = new RegExp('/(blocks5-' + BUILD + '\\.(js|wasm|data)|' +
                      'touch_controls-' + PAD + '\\.js)$');

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
		// Only this build's own payload belongs in this cache store. While a
		// new worker installs, the old one is still answering, and without this
		// line it would pull the new build's files into its own store, which is
		// about to be deleted - both payloads on disk for the duration.
		// It lets anything stamped for another build through untouched instead.
		if (!MINE.test(url.pathname)) return;

		e.respondWith(caches.open(CACHE).then(function (c) {
			return c.match(req, { ignoreSearch: true }).then(function (hit) {
				return hit || fetch(req).then(function (res) { return keep(c, res); });
			});
		}));
		return;
	}

	// Network first - but a worker cannot make that true on its own, and it is
	// worth knowing why before trying. A subresource the browser's own HTTP
	// cache still considers fresh is served from there without this handler
	// ever running: measured on a reload, touch_controls.js came back with
	// workerStart 0, transferSize 0 and deliveryType "cache". So fetching it
	// here with cache 'no-cache' fixes nothing - by the time this code runs,
	// the HTTP cache has already declined to answer. What decides it is the
	// Cache-Control the server sends, which is why WebBuild/htaccess names
	// every unstamped file and not a chosen few. This branch is what keeps the
	// page working offline; freshness is the header's job.
	e.respondWith(caches.open(CACHE).then(function (c) {
		return fetch(req).then(function (res) {
			return keep(c, res);
		}).catch(function () {
			// No network: then whatever came in at install time.
			return c.match(req, { ignoreSearch: true }).then(function (hit) {
				return hit || c.match('./index.html');
			});
		});
	}));
});
