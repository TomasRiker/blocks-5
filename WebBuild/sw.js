// sw.js - the offline cache for the browser build.
//
// The three payload files must never be mixed: blocks5-<build>.js holds
// absolute byte offsets into blocks5-<build>.data, and its EM_ASM fragments sit
// at addresses that fit exactly one blocks5-<build>.wasm. A mismatched set
// aborts with "No EM_ASM constant found at address ...".
//
// The build's stamp in those filenames makes each such URL immutable, so no
// cache on the way (this one, the browser's, a proxy, mod_pagespeed) can serve
// one build's JavaScript beside another's wasm. Hence the two halves below are
// cached in opposite directions:
//
//   stamped payload   cache first: it is immutable, so the network could only
//                     confirm what is already here.
//   everything else   network first, cache as the fallback. index.html cannot
//                     carry a stamp - it is where the current stamp is written
//                     down - so serving it from the cache would hide every new
//                     build. Offline it still comes from the cache.
//
// skipWaiting and clients.claim are safe for the same reason: a booted page
// holds stamped URLs, so a worker taking over cannot hand it another build
// halfway through.
//
// build.sh replaces %%VERSION%% with a hash of the three files, so the cache
// name changes exactly when the payload does.
var BUILD = '%%VERSION%%';
// The pad's own stamp, a hash of touch_controls.js, which BUILD does not cover.
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
		// Only this build's own payload belongs in this store. While a new
		// worker installs, the old one still answers, and without this line
		// it would pull the new build's files into its own store, which is
		// about to be deleted. Anything stamped for another build passes
		// through untouched.
		if (!MINE.test(url.pathname)) return;

		e.respondWith(caches.open(CACHE).then(function (c) {
			return c.match(req, { ignoreSearch: true }).then(function (hit) {
				return hit || fetch(req).then(function (res) { return keep(c, res); });
			});
		}));
		return;
	}

	// Network first, as far as a worker can make it so. A subresource the
	// browser's HTTP cache still considers fresh is served from there without
	// this handler running (measured on a reload: workerStart 0, transferSize
	// 0, deliveryType "cache"), so a cache: 'no-cache' fetch here would change
	// nothing. Freshness is the job of the server's Cache-Control, which is
	// why WebBuild/htaccess names every unstamped file; this branch keeps the
	// page working offline.
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
