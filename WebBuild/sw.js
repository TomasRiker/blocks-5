// sw.js - the offline cache for the browser build.
//
// The three payload files belong together. blocks5.js carries a table of
// absolute byte offsets into blocks5.data:
//
//   loadPackage({files:[{filename:"/data.zip",start:4,end:3191909}, ...
//
// so a new bundle served beside an old table slices every preloaded file in the
// wrong place, and because the entries are consecutive, one byte of difference
// moves the campaign, the skins and the fonts along with it. That is exactly
// what roadmap item 20 turned out to be, and a cache is a second place where it
// could happen - this time on somebody's phone, where nobody can see it. Two
// rules keep the set together:
//
//   - install uses cache.addAll for the payload, which is all-or-nothing: a
//     version is either completely in the cache or the install fails and the
//     worker that was already there keeps serving the old, consistent set.
//   - there is deliberately no skipWaiting() and no clients.claim(). A page
//     keeps the worker that controlled it when it loaded, so what it is served
//     cannot change halfway through booting. A new version takes over on the
//     next load that starts without an old worker still in charge - closing the
//     tab and opening it again.
//
// %%VERSION%% is replaced by build.sh with a hash of the three files, so the
// cache name changes exactly when the payload does and never otherwise.
var CACHE = 'blocks5-%%VERSION%%';

// Without these the game cannot start, and a half-cached set is worse than
// none: they are fetched as one unit.
var PAYLOAD = ['./index.html', './blocks5.js', './blocks5.wasm', './blocks5.data'];

// Nice to have. './' is what a home-screen launch asks for, but a server that
// does not serve a directory index would fail the whole install over it.
var EXTRA = ['./', './blocks5.html', './manifest.json', './icon.png'];

self.addEventListener('install', function (e) {
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
	}));
});

self.addEventListener('fetch', function (e) {
	var req = e.request;
	if (req.method !== 'GET') return;
	if (new URL(req.url).origin !== self.location.origin) return;

	e.respondWith(caches.open(CACHE).then(function (c) {
		return c.match(req, { ignoreSearch: true }).then(function (hit) {
			if (hit) return hit;
			return fetch(req).then(function (res) {
				// Only a plain, successful, same-origin answer is worth keeping;
				// an opaque or partial one would poison the cache.
				if (res && res.status === 200 && res.type === 'basic') c.put(req, res.clone());
				return res;
			});
		});
	}));
});
