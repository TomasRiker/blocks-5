// mobile.js - what a phone gets, checked on an emulated one.
//
//   cd WebBuild && ./build.sh hooks && cd test && node mobile.js
//
// smoke.js walks the game; this one walks the page around it. It loads
// index.html - the file that ships, and the only one that registers the service
// worker - in Chromium's mobile emulation, which is what makes the viewport
// meta mean anything: without isMobile the browser lays out at the window width
// and the tag has nothing to do.
//
// Checked here, in order of how much each one hurt when it was missing:
//
//   1. the layout viewport is the device width, not the ~980px default
//   2. the page cannot be scrolled or zoomed away from the game
//   3. the canvas covers the viewport
//   4. a real finger - touchStart, wait, touchEnd - reaches a GUI button
//   5. the manifest is served, parses, and says what an install needs
//   6. the service worker installs and has the payload in its cache
//   7. with the network off, a reload still reaches the menu
//
// Number four is the one that needs the wait in the middle. The game samples
// the mouse once per 20 ms logic tick, so a tap that presses and releases in
// the same millisecond falls between two samples and is never seen - the same
// trap as page.mouse.click(), and page.touchscreen.tap() has it too.
const { chromium } = require('playwright');
const { spawn } = require('child_process');
const path = require('path');
const fs = require('fs');

const PORT = 8098;
const DIR = path.join(__dirname, '..', 'build-test');
const CHROME = process.env.PLAYWRIGHT_CHROMIUM || '/opt/pw-browsers/chromium';

// A Pixel 7 held sideways: the shape somebody actually plays this in.
const PHONE = {
	viewport: { width: 915, height: 412 },
	deviceScaleFactor: 2.625,
	isMobile: true,
	hasTouch: true,
	userAgent: 'Mozilla/5.0 (Linux; Android 13; Pixel 7) AppleWebKit/537.36 ' +
	           '(KHTML, like Gecko) Chrome/120.0.0.0 Mobile Safari/537.36',
};

let problems = 0;
const ok = m => console.log('  . ' + m);
const bad = m => { console.log('  ! ' + m); problems++; };
const wait = ms => new Promise(r => setTimeout(r, ms));

// The guarded form from harness.js, and it has to be guarded: calling a
// kept-alive export before the wasm is instantiated reaches a stub that aborts,
// and an abort in this Emscripten hangs the page instead of throwing - which
// looks exactly like a slow boot until the timeout runs out.
async function dump(page) {
	const json = await page.evaluate(() => {
		if (typeof Module === 'undefined') return null;
		if (typeof runtimeInitialized === 'undefined' || !runtimeInitialized) return null;
		if (!Module._blocks5_testDump) return null;
		try { Module._blocks5_testDump(); } catch (e) { return null; }
		return Module.b5_test || null;
	});
	if (!json) throw new Error('the runtime is not up yet');
	return JSON.parse(json);
}

// want may be a state name or a predicate over the dump.
async function waitFor(page, want, what, timeoutMs) {
	const test = typeof want === 'function' ? want : (d => d.state === want);
	const until = Date.now() + (timeoutMs || 180000);
	for (;;) {
		try {
			const d = await dump(page);
			if (test(d)) return d;
		} catch (e) { /* not up yet */ }
		if (Date.now() > until) throw new Error('timed out waiting for ' + what);
		await wait(1000);
	}
}

// "Booted" is the first dump that answers with a state at all. The GUI tree is
// still empty in GS_Loading, so it cannot be part of the condition here.
const booted = d => !!d.state;

// A tap that the game can actually see: down, hold past a logic tick, up.
async function tap(page, cdp, x, y) {
	const point = [{ x: Math.round(x), y: Math.round(y), radiusX: 12, radiusY: 12, force: 1 }];
	await cdp.send('Input.dispatchTouchEvent', { type: 'touchStart', touchPoints: point });
	await wait(400);
	await cdp.send('Input.dispatchTouchEvent', { type: 'touchEnd', touchPoints: [] });
	await wait(1200);
}

// Game window coordinates to page coordinates, exactly as harness.js does it.
async function toPage(page, win) {
	const box = await page.evaluate(() => {
		const r = Module.canvas.getBoundingClientRect();
		return { left: r.left, top: r.top, cssW: r.width, cssH: r.height,
		         w: Module.canvas.width, h: Module.canvas.height };
	});
	return {
		x: box.left + (win[0] + win[2] / 2) * (box.cssW / box.w),
		y: box.top + (win[1] + win[3] / 2) * (box.cssH / box.h),
	};
}

(async () => {
	if (!fs.existsSync(path.join(DIR, 'index.html'))) {
		console.log('no index.html in ' + DIR + ' - run ./build.sh hooks first');
		process.exit(2);
	}
	const server = spawn('python3', ['-m', 'http.server', String(PORT)],
	                     { cwd: DIR, stdio: 'ignore', detached: true });
	await wait(1500);

	const browser = await chromium.launch({
		executablePath: CHROME,
		args: ['--use-gl=swiftshader', '--enable-unsafe-swiftshader', '--no-sandbox'],
	});
	const context = await browser.newContext(PHONE);
	const page = await context.newPage();
	const cdp = await context.newCDPSession(page);
	const url = 'http://127.0.0.1:' + PORT + '/index.html';

	try {
		await page.goto(url);

		// --- 1. the viewport meta ------------------------------------------
		const vp = await page.evaluate(() => ({
			inner: window.innerWidth,
			scale: window.visualViewport ? window.visualViewport.scale : 1,
		}));
		if (vp.inner === PHONE.viewport.width) ok('layout viewport is ' + vp.inner + 'px, the device width');
		else bad('layout viewport is ' + vp.inner + 'px, expected ' + PHONE.viewport.width +
		         ' - the viewport meta did not take');

		// --- 5. the manifest ------------------------------------------------
		const man = await (await page.request.get(url.replace('index.html', 'manifest.json'))).json();
		const icons = man.icons || [];
		const bigEnough = icons.some(i => parseInt(i.sizes) >= 192 && /(^|\s)any(\s|$)/.test(i.purpose || 'any'));
		if (man.name && man.start_url && man.display && bigEnough) {
			ok('manifest: "' + man.short_name + '", ' + man.display + ', ' + icons.length + ' icons');
		} else {
			bad('manifest is missing something an install needs');
		}
		// A launcher crops a maskable icon to a shape of its own choosing, so one
		// has to exist and it must not be the full-bleed drawing - that would come
		// back with its rim cut off.
		const maskable = icons.filter(i => /maskable/.test(i.purpose || ''));
		if (maskable.length) {
			const shot = await page.request.get(url.replace('index.html', maskable[0].src));
			ok('a maskable icon is declared (' + maskable[0].src + ', ' +
			   (shot.ok() ? 'served' : 'MISSING') + ')');
			if (!shot.ok()) bad(maskable[0].src + ' is declared but not served');
		} else {
			bad('no maskable icon - a launcher will shrink the drawing onto a white square');
		}

		const boot = await waitFor(page, booted, 'the runtime', 240000);
		ok('the runtime came up in ' + boot.state);

		// --- 3. the canvas --------------------------------------------------
		const fit = await page.evaluate(() => {
			const r = Module.canvas.getBoundingClientRect();
			return { w: r.width, h: r.height, iw: window.innerWidth, ih: window.innerHeight,
			         scrollH: document.documentElement.scrollHeight };
		});
		if (Math.abs(fit.w - fit.iw) <= 1 && Math.abs(fit.h - fit.ih) <= 1) {
			ok('the canvas covers the viewport (' + Math.round(fit.w) + ' x ' + Math.round(fit.h) + ')');
		} else {
			bad('canvas ' + Math.round(fit.w) + ' x ' + Math.round(fit.h) +
			    ' against a viewport of ' + fit.iw + ' x ' + fit.ih);
		}

		// --- 2. nothing to scroll or zoom -----------------------------------
		const style = await page.evaluate(() => {
			const b = getComputedStyle(document.body);
			return { touch: b.touchAction, over: b.overflow };
		});
		if (style.touch === 'none') ok('touch-action: none - the browser keeps its gestures to itself');
		else bad('touch-action is "' + style.touch + '", expected none');
		if (fit.scrollH <= fit.ih + 1) ok('the page has nothing to scroll');
		else bad('the page scrolls: ' + fit.scrollH + 'px of content in ' + fit.ih + 'px');

		// --- 4. a finger reaches a button ------------------------------------
		// Past the title screen first: a tap anywhere starts the game.
		let d = await dump(page);
		let p = await toPage(page, [0, 0, d.display[2], d.display[3]]);
		await tap(page, cdp, p.x, p.y);
		d = await waitFor(page, x => x.state === 'GS_Menu' && x.elements.length, 'the main menu', 240000);
		ok('a tap on the title screen started the game');

		const crt = d.elements.find(e => e.path === 'Menu.CrtPane.Crt.NoThanks');
		if (crt && crt.shown) {
			p = await toPage(page, crt.win);
			await tap(page, cdp, p.x, p.y);
			d = await dump(page);
		}

		const target = d.elements.find(e => e.path === 'Menu.Options');
		p = await toPage(page, target.win);
		await tap(page, cdp, p.x, p.y);
		d = await dump(page);
		const opts = d.elements.find(e => e.path === 'OptionsPane.Options');
		if (opts && opts.shown) ok('a tap on Menu.Options opened the options');
		else bad('a tap on Menu.Options did not open the options');

		// Back out again, so the reload below starts from the menu.
		await page.keyboard.press('Escape');
		await wait(1500);

		// --- 6. the service worker ------------------------------------------
		const sw = await page.evaluate(async () => {
			const reg = await navigator.serviceWorker.ready;
			const names = await caches.keys();
			let held = [];
			for (const n of names) {
				const c = await caches.open(n);
				held = held.concat((await c.keys()).map(r => new URL(r.url).pathname));
			}
			return { scope: reg.scope, names: names, held: held };
		});
		const need = ['/blocks5.js', '/blocks5.wasm', '/blocks5.data', '/index.html'];
		const missing = need.filter(f => !sw.held.includes(f));
		if (sw.names.length === 1 && !missing.length) {
			ok('service worker: cache "' + sw.names[0] + '" holds all ' + sw.held.length + ' files');
		} else {
			bad('service worker cache ' + JSON.stringify(sw.names) + ' is missing ' + missing.join(', '));
		}

		// --- 7. offline ------------------------------------------------------
		await context.setOffline(true);
		await page.reload();
		const off = await waitFor(page, booted, 'the offline boot', 240000);
		ok('with the network off, a reload still boots (' + off.state + ')');
		await context.setOffline(false);
	} catch (e) {
		bad(e.message);
	}

	await browser.close();
	try { process.kill(-server.pid); } catch (e) {}

	console.log();
	if (problems === 0) { console.log('OK'); process.exit(0); }
	console.log(problems + ' problem(s)');
	process.exit(1);
})();
