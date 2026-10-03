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
// Checked here, in order of how much each one hurt by its absence:
//
//   1. the layout viewport is the device width, not the ~980px default
//   2. the page cannot be scrolled or zoomed away from the game
//   3. the canvas covers the viewport
//   4. a real finger - touchStart, wait, touchEnd - reaches a GUI button,
//      and one that just misses it is moved onto it, where a mouse misses;
//      dragged on a list, it scrolls it without selecting, and after a drag
//      the system cancels the next touch still presses what it lands on
//   5. the manifest is served, parses, and says what an install needs
//   6. the service worker installs and has the payload in its cache
//   7. with the network off, a reload still reaches the menu
//   8. that first tap takes the page fullscreen too, asks for landscape and
//      locks Escape, and the pad offers a button to toggle it
//   9. a finger's tap on one of the game's text fields opens the page's text
//      sheet with the field's text; OK hands back what was typed, cut down to
//      Latin-1, and Cancel nothing; keys typed there reach neither the game
//      nor the pad; a drag or a mouse opens no sheet
//
// Number four is the one that needs the wait in the middle. The game samples
// the mouse once per 20 ms logic tick; a tap that presses and releases in the
// same millisecond therefore falls between two samples and is never seen - the
// same trap as page.mouse.click(), and page.touchscreen.tap() has it too.
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
// still empty in GS_Loading and therefore cannot be part of the condition here.
const booted = d => !!d.state;

// A tap that the game can actually see: down, hold past a logic tick, up.
async function tap(page, cdp, x, y) {
	const point = [{ x: Math.round(x), y: Math.round(y), radiusX: 12, radiusY: 12, force: 1 }];
	await cdp.send('Input.dispatchTouchEvent', { type: 'touchStart', touchPoints: point });
	await wait(400);
	await cdp.send('Input.dispatchTouchEvent', { type: 'touchEnd', touchPoints: [] });
	await wait(1200);
}

// A finger put down, slid in five steps and lifted after a rest there, so
// that it lifts still and the list does not glide on.
async function drag(page, cdp, from, to) {
	const at = p => [{ x: Math.round(p.x), y: Math.round(p.y), radiusX: 12, radiusY: 12, force: 1 }];
	await cdp.send('Input.dispatchTouchEvent', { type: 'touchStart', touchPoints: at(from) });
	await wait(400);
	for (let i = 1; i <= 5; i++) {
		const p = { x: from.x + (to.x - from.x) * i / 5, y: from.y + (to.y - from.y) * i / 5 };
		await cdp.send('Input.dispatchTouchEvent', { type: 'touchMove', touchPoints: at(p) });
		await wait(150);
	}
	await wait(600);
	await cdp.send('Input.dispatchTouchEvent', { type: 'touchEnd', touchPoints: [] });
	await wait(1200);
}

// A finger on a button of the page's own, held briefly: the four tenths of a
// second tap() holds for the game's sake come close to a long press.
async function tapElement(page, cdp, id) {
	const r = await page.evaluate(i => {
		const b = document.getElementById(i).getBoundingClientRect();
		return { x: b.left + b.width / 2, y: b.top + b.height / 2 };
	}, id);
	const point = [{ x: Math.round(r.x), y: Math.round(r.y), radiusX: 12, radiusY: 12, force: 1 }];
	await cdp.send('Input.dispatchTouchEvent', { type: 'touchStart', touchPoints: point });
	await wait(80);
	await cdp.send('Input.dispatchTouchEvent', { type: 'touchEnd', touchPoints: [] });
	await wait(1200);
}

// The text sheet as the page shows it, and the field that has the focus.
async function sheetState(page) {
	return page.evaluate(() => {
		const f = document.activeElement;
		return {
			open: getComputedStyle(document.getElementById('b5_sheet')).display !== 'none',
			focus: f ? f.id : '',
			value: f && typeof f.value === 'string' ? f.value : null,
			caption: document.getElementById('b5_sheet_caption').textContent,
			capitalize: f && f.getAttribute ? f.getAttribute('autocapitalize') : null,
			spellcheck: f ? f.spellcheck : null,
			pad: !!(window.b5pad && window.b5pad.isVisible()),
		};
	});
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

// A game coordinate to page coordinates, through the present rectangle the
// dump reports, as b5_mouseAt does natively.
async function gameToPage(page, d, gx, gy) {
	const box = await page.evaluate(() => {
		const r = Module.canvas.getBoundingClientRect();
		return { left: r.left, top: r.top, cssW: r.width, cssH: r.height,
		         w: Module.canvas.width, h: Module.canvas.height };
	});
	const wx = d.present[0] + (gx + 0.5) * d.present[2] / d.screen[2];
	const wy = d.present[1] + (gy + 0.5) * d.present[3] / d.screen[3];
	return { x: box.left + wx * (box.cssW / box.w), y: box.top + wy * (box.cssH / box.h) };
}

(async () => {
	if (!fs.existsSync(path.join(DIR, 'index.html'))) {
		console.log('no index.html in ' + DIR + ' - run ./build.sh hooks first');
		process.exit(2);
	}
	// server.py and not python3 -m http.server: the built-in one sends no
	// Cache-Control at all, which is a configuration nobody deploys, and every
	// caching check below would then be asking about the wrong server. It reads
	// the rules out of WebBuild/htaccess, so what the browser is told here is
	// what a real one is told.
	const server = spawn('python3', [path.join(__dirname, 'server.py'),
	                                 String(PORT), DIR],
	                     { stdio: 'ignore', detached: true });
	await wait(1500);

	const browser = await chromium.launch({
		executablePath: CHROME,
		args: ['--use-gl=swiftshader', '--enable-unsafe-swiftshader', '--no-sandbox'],
	});
	const context = await browser.newContext(PHONE);
	// Whether the landscape lock is asked for is the only part of it that can be
	// checked here: a headless browser has no orientation to turn, which means
	// the lock is refused whatever the page does. Recording the call still
	// catches the regression that matters - the request going missing - and
	// calling through keeps the refusal, which the page must swallow.
	await context.addInitScript(() => {
		window.__b5locks = [];
		// The keyboard lock the same way: Escape has to stay with the game.
		if (navigator.keyboard && navigator.keyboard.lock) {
			const klock = navigator.keyboard.lock;
			Object.defineProperty(navigator.keyboard, 'lock', { configurable: true,
				value: function (keys) {
					window.__b5locks.push('keys:' + (keys || []).join(','));
					return klock.call(this, keys);
				} });
		}
		if (!window.screen || !screen.orientation) return;
		const lock = screen.orientation.lock, unlock = screen.orientation.unlock;
		Object.defineProperty(screen.orientation, 'lock', { configurable: true,
			value: function (what) {
				window.__b5locks.push(what);
				return lock ? lock.call(this, what) : Promise.reject(new Error('unsupported'));
			} });
		Object.defineProperty(screen.orientation, 'unlock', { configurable: true,
			value: function () {
				window.__b5locks.push('unlock');
				if (unlock) unlock.call(this);
			} });
	});
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
		// A launcher crops a maskable icon to a shape of its own choosing, which
		// means one has to exist and it must not be the full-bleed drawing - that
		// would come back with its rim cut off.
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

		// --- 8. and took the page fullscreen ---------------------------------
		// The first gesture takes it, on every device, and this tap is the first.
		// Portrait is unplayable at this size, hence the lock that goes with it,
		// and Escape is locked so that the menu gets it and not the browser.
		//
		// It has to be the ROOT element and not the canvas: the browser paints
		// only the fullscreen element and what is inside it; with the canvas
		// promoted the on-screen controls beside it are simply not drawn - while
		// still reporting a full-size bounding rect, which is why measuring them
		// would not have noticed.
		const full = await page.evaluate(() => ({
			el: (document.fullscreenElement || document.webkitFullscreenElement || {}).tagName || null,
			locks: window.__b5locks.slice(),
		}));
		if (full.el === 'HTML') ok('the same tap took the page fullscreen');
		else bad('the fullscreen element is ' + full.el + ', expected HTML');
		if (full.locks.indexOf('landscape') >= 0) ok('landscape was requested');
		else bad('no landscape lock was requested');
		if (full.locks.indexOf('keys:Escape') >= 0) ok('Escape was locked to the page');
		else bad('Escape was not locked: ' + JSON.stringify(full.locks));
		// Shown, and with a rectangle on the screen: a hidden pad root or an
		// unsized button both come out as an empty rectangle.
		const padButton = await page.evaluate(() => {
			const g = document.querySelector('#b5pad .b5corners');
			if (!g) return null;
			const r = g.parentNode.getBoundingClientRect();
			return { pad: window.b5pad.isVisible(), w: r.width, h: r.height,
			         inside: r.left >= 0 && r.top >= 0 &&
			                 r.right <= innerWidth && r.bottom <= innerHeight };
		});
		if (padButton && padButton.pad && padButton.w > 0 && padButton.h > 0 && padButton.inside)
			ok('the pad offers a fullscreen button (' + padButton.w + 'x' + padButton.h + ')');
		else bad('the pad has no visible fullscreen button: ' + JSON.stringify(padButton));

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

		// A finger scrolls a list by dragging it, and selects only with a tap
		// (GUI_ListBox): the options' list of actions, dragged up 50 game
		// pixels by real touches, follows the finger by that less the slop.
		const actions = () => d.elements.find(e => e.path === 'OptionsPane.Options.Actions');
		let list = actions();
		if (list && list.shown) {
			const r = list.rect;
			await drag(page, cdp, await gameToPage(page, d, r[0] + 40, r[1] + 60),
			           await gameToPage(page, d, r[0] + 40, r[1] + 10));
			d = await dump(page);
			list = actions();
			if (list.scroll > 30 && list.scroll < 50 && list.selection === -1)
				ok('a finger dragged up the list of actions scrolls it ' + list.scroll + ' pixels and selects nothing');
			else bad('a finger dragged up the list of actions left it scrolled ' + list.scroll +
			         ' with item ' + list.selection + ' selected');
			// The middle of the item drawn about 30 pixels down: a CSS pixel is
			// more than a game pixel here, and a tap aimed at an item's edge
			// can land on the next.
			const scrolled = list.scroll, lh = list.lineHeight;
			const want = Math.floor((30 - 2 + scrolled) / lh);
			const item = await gameToPage(page, d, r[0] + 40, r[1] + 2 - scrolled + want * lh + (lh >> 1));
			await tap(page, cdp, item.x, item.y);
			d = await dump(page);
			list = actions();
			if (list.selection === want && list.scroll === scrolled)
				ok('and a tap on it selects the item under the finger (' + want + ')');
			else bad('a tap on the list of actions selected item ' + list.selection + ' at ' + list.scroll +
			         ', not ' + want + ' at ' + scrolled);

			// A touch the system cancels never lifts as far as Emscripten's
			// SDL can tell - it hands on no touchcancel, so the finger and the
			// button stay down - and a phone gives the next touch the same
			// identifier, for which SDL then makes no press: that tap must
			// still arrive, and must not take the list along. A drag down the
			// list cancelled, then a tap on the options' Cancel.
			const at = p => [{ x: Math.round(p.x), y: Math.round(p.y), radiusX: 12, radiusY: 12, force: 1 }];
			const down = await gameToPage(page, d, r[0] + 40, r[1] + 15);
			await cdp.send('Input.dispatchTouchEvent', { type: 'touchStart', touchPoints: at(down) });
			await wait(400);
			for (let i = 1; i <= 3; i++) {
				await cdp.send('Input.dispatchTouchEvent', { type: 'touchMove', touchPoints: at({ x: down.x, y: down.y + 10 * i }) });
				await wait(150);
			}
			await wait(400);
			await cdp.send('Input.dispatchTouchEvent', { type: 'touchCancel', touchPoints: [] });
			await wait(600);
			d = await dump(page);
			const cancelled = actions().scroll;
			const cancel = await toPage(page, d.elements.find(e => e.path === 'OptionsPane.Options.Cancel').win);
			await tap(page, cdp, cancel.x, cancel.y);
			d = await dump(page);
			if (cancelled < scrolled && actions().scroll === cancelled)
				ok('a list whose touch was cancelled stays put under the next touch (' + cancelled + ')');
			else bad('a list whose touch was cancelled at ' + cancelled + ' (from ' + scrolled + ') went to ' +
			         actions().scroll + ' under the next touch');
			if (!d.elements.find(e => e.path === 'OptionsPane.Options').shown)
				ok('and that touch arrives: Cancel closed the options');
			else bad('the touch after a cancelled one did not press Cancel');
		}
		else bad('the options show no list of actions');

		// Back out again, where the options are still open - the reload below
		// has to start from the menu, and Escape in the menu quits.
		if (d.elements.find(e => e.path === 'OptionsPane.Options').shown) {
			await page.keyboard.press('Escape');
			await wait(1500);
		}

		// A finger is not a point (GUI::pickTouchTarget). The corner of the
		// options button's 80x80 cell lies outside the square, inset 8 in it,
		// that the button is hit on: a mouse there misses, a finger there is
		// moved onto the button. The picker's answer first, then a real touch
		// and a real mouse.
		d = await dump(page);
		const corner = await gameToPage(page, d, 471, 55);
		const picked = await page.evaluate(() => {
			Module._blocks5_testTouchAt(471, 55);
			return Module.b5_touch;
		});
		if (/^Menu\.Options \d+ \d+ moved$/.test(picked))
			ok('a finger at the corner of the options button is moved onto it (' + picked + ')');
		else bad('a finger at the corner of the options button presses ' + picked);
		await tap(page, cdp, corner.x, corner.y);
		d = await dump(page);
		const near = d.elements.find(e => e.path === 'OptionsPane.Options');
		if (near && near.shown) ok('and a tap there opens the options');
		else bad('a tap at the corner of the options button did not open them');
		if (near && near.shown) { await page.keyboard.press('Escape'); await wait(1500); }
		d = await dump(page);
		const closed = d.elements.find(e => e.path === 'OptionsPane.Options');
		if (closed && closed.shown) bad('Escape did not close the options again');

		await page.mouse.move(corner.x, corner.y);
		await wait(400);
		await page.mouse.down();
		await wait(400);
		await page.mouse.up();
		await wait(1200);
		d = await dump(page);
		const missed = d.elements.find(e => e.path === 'OptionsPane.Options');
		if (missed && !missed.shown) ok('a mouse there misses them, as before');
		else {
			bad('a mouse at the corner of the options button opened them');
			await page.keyboard.press('Escape');
			await wait(1500);
		}

		// Not vacuous: the mouse was there, and a press of it on the button
		// does open them.
		if (!d.cursor || Math.abs(d.cursor[0] - 471) > 1 || Math.abs(d.cursor[1] - 55) > 1)
			bad('the mouse was at ' + JSON.stringify(d.cursor) + ', not at 471,55');
		const button = d.elements.find(e => e.path === 'Menu.Options');
		const middle = await gameToPage(page, d, button.rect[0] + (button.rect[2] >> 1), button.rect[1] + (button.rect[3] >> 1));
		await page.mouse.move(middle.x, middle.y);
		await wait(400);
		await page.mouse.down();
		await wait(400);
		await page.mouse.up();
		await wait(1200);
		d = await dump(page);
		const opened = d.elements.find(e => e.path === 'OptionsPane.Options');
		if (opened && opened.shown) {
			ok('and a mouse on the button opens them, so its presses arrive');
			await page.keyboard.press('Escape');
			await wait(1500);
		}
		else bad('a mouse on the options button did not open them');

		// --- 9. the text sheet -----------------------------------------------
		// A phone shows its keyboard only for a field of the page, so a finger's
		// tap on a text field of the game's opens a sheet with a real one
		// (pre.js, web_textsheet.cpp). The campaign editor has all three kinds:
		// a title, a description of several lines, and a file name, which is
		// typed exactly. Text goes in as a phone's keyboard puts it there,
		// through insertText: an edit of the field and no key at all.
		d = await dump(page);
		p = await toPage(page, d.elements.find(e => e.path === 'Menu.CampaignEditor').win);
		await tap(page, cdp, p.x, p.y);
		d = await waitFor(page, x => x.state === 'GS_CampaignEditor' &&
		                  x.elements.some(e => e.path === 'CampaignEditor.Title' && e.shown),
		                  'the campaign editor', 60000);
		const textField = name => d.elements.find(e => e.path === 'CampaignEditor.' + name);
		const selectAll = () => page.evaluate(() => {
			const f = document.activeElement;
			if (f && f.select) f.select();
		});
		const typeText = text => cdp.send('Input.insertText', { text: text });

		const title = textField('Title');
		const titleAt = await toPage(page, title.win);
		await tap(page, cdp, titleAt.x, titleAt.y);
		let sheet = await sheetState(page);
		if (sheet.open && sheet.focus === 'b5_sheet_line' && sheet.value === title.value &&
		    sheet.caption === 'Title:' && sheet.capitalize === 'sentences' && sheet.spellcheck)
			ok('a tap on the title opens the sheet: "' + sheet.caption + '", the field focused and holding "' + sheet.value + '"');
		else bad('a tap on the title "' + title.value + '" gave the sheet ' + JSON.stringify(sheet));

		// Typographic quotes, a dash and an ellipsis are what a phone's keyboard
		// makes of the plain ones; a letter beyond Latin-1 keeps its base letter
		// where it has one, and an emoji has none.
		await selectAll();
		await typeText('B\u00e4renh\u00f6hle \u2013 \u201eBob\u2019s\u201c \u0142\u0101\ud83d\ude00\u2026');
		const typed = 'B\u00e4renh\u00f6hle - "Bob\'s" a...';
		await tapElement(page, cdp, 'b5_sheet_ok');
		d = await dump(page);
		sheet = await sheetState(page);
		if (!sheet.open && sheet.focus !== 'b5_sheet_line' && textField('Title').value === typed)
			ok('OK puts it into the game\'s title, cut down to Latin-1: "' + typed + '"');
		else bad('after OK the title is "' + textField('Title').value + '", not "' + typed + '", and the sheet ' +
		         JSON.stringify(sheet));

		await tap(page, cdp, titleAt.x, titleAt.y);
		sheet = await sheetState(page);
		if (sheet.open && sheet.value === typed) ok('tapped again, the sheet holds the new title');
		else bad('tapped again, the sheet holds ' + JSON.stringify(sheet.value));
		await selectAll();
		await typeText('Thrown away');
		await tapElement(page, cdp, 'b5_sheet_cancel');
		d = await dump(page);
		if (!(await sheetState(page)).open && textField('Title').value === typed) ok('Cancel leaves the title as it was');
		else bad('after Cancel the title is "' + textField('Title').value + '"');

		// Real keys, as a phone's keyboard sends some and a tablet's keyboard
		// all: they belong to the sheet's field. The game's title has the
		// focus behind it and must not take them, and the pad must stay - it
		// goes away for a real key anywhere else. Enter is OK.
		await tap(page, cdp, titleAt.x, titleAt.y);
		await selectAll();
		await page.keyboard.type('Keys');
		d = await dump(page);
		sheet = await sheetState(page);
		if (sheet.value === 'Keys' && textField('Title').value === typed && d.focus === 'CampaignEditor.Title')
			ok('keys typed in the sheet go to its field, not to the game\'s title behind it');
		else bad('keys typed in the sheet: its field "' + sheet.value + '", the game\'s title "' +
		         textField('Title').value + '", focus ' + d.focus);
		if (sheet.pad) ok('and the pad stays up');
		else bad('a key typed in the sheet hid the pad');
		await page.keyboard.press('Enter');
		await wait(1200);
		d = await dump(page);
		if (!(await sheetState(page)).open && textField('Title').value === 'Keys') ok('Enter is OK');
		else bad('after Enter the title is "' + textField('Title').value + '"');

		// Escape is Cancel, and goes no further: in the editor it would quit.
		await tap(page, cdp, titleAt.x, titleAt.y);
		await selectAll();
		await page.keyboard.type('Escaped');
		await page.keyboard.press('Escape');
		await wait(1200);
		d = await dump(page);
		const asked = d.elements.find(e => e.path === 'CampaignEditor.MessageBoxPane');
		if (!(await sheetState(page)).open && textField('Title').value === 'Keys' &&
		    d.state === 'GS_CampaignEditor' && !(asked && asked.shown))
			ok('Escape is Cancel and does not reach the editor');
		else bad('after Escape: ' + d.state + ', the title "' + textField('Title').value + '", the question ' +
		         (asked && asked.shown ? 'up' : 'down'));

		// The description, in a field of several lines, where Enter is a line
		// break and stays one.
		const description = textField('Description');
		const descriptionAt = await toPage(page, description.win);
		await tap(page, cdp, descriptionAt.x, descriptionAt.y);
		sheet = await sheetState(page);
		if (sheet.open && sheet.focus === 'b5_sheet_lines' && sheet.value === description.value &&
		    sheet.caption === 'Description:')
			ok('a tap on the description opens the sheet with a field of several lines');
		else bad('a tap on the description gave the sheet ' + JSON.stringify(sheet));
		await selectAll();
		await typeText('First line');
		await page.keyboard.press('Enter');
		await typeText('second line');
		await tapElement(page, cdp, 'b5_sheet_ok');
		d = await dump(page);
		if (textField('Description').value === 'First line\nsecond line')
			ok('and OK puts both lines into the game\'s description');
		else bad('after OK the description is ' + JSON.stringify(textField('Description').value));

		// A name typed exactly: no capital letter, no correction.
		const filename = await toPage(page, textField('Filename').win);
		await tap(page, cdp, filename.x, filename.y);
		sheet = await sheetState(page);
		if (sheet.open && sheet.caption === 'Filename:' && sheet.capitalize === 'off' && !sheet.spellcheck)
			ok('the file name\'s sheet capitalizes and corrects nothing');
		else bad('the file name\'s sheet: ' + JSON.stringify(sheet));
		if (sheet.open) await tapElement(page, cdp, 'b5_sheet_cancel');

		// A drag is no tap: the description pans under it and no sheet opens.
		// Clear of the box's bottom rows, which are its horizontal scroll bar.
		await drag(page, cdp, { x: descriptionAt.x, y: descriptionAt.y + 10 },
		           { x: descriptionAt.x, y: descriptionAt.y - 30 });
		if (!(await sheetState(page)).open) ok('a finger dragged over the description opens no sheet');
		else {
			bad('a finger dragged over the description opened the sheet');
			await tapElement(page, cdp, 'b5_sheet_cancel');
		}

		// Nor does a mouse, which has a keyboard beside it; its click still
		// puts the focus in the field.
		await page.mouse.move(titleAt.x, titleAt.y);
		await wait(400);
		await page.mouse.down();
		await wait(400);
		await page.mouse.up();
		await wait(1200);
		d = await dump(page);
		if (!(await sheetState(page)).open && d.focus === 'CampaignEditor.Title')
			ok('a mouse on the title focuses it and opens no sheet');
		else bad('a mouse on the title: the sheet ' + ((await sheetState(page)).open ? 'opened' : 'stayed shut') +
		         ', the focus on ' + d.focus);

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
		// The three payload files carry the build's stamp in their names, which
		// keeps any cache on the way from putting one build's JavaScript file
		// beside another build's wasm. The cache name carries the same stamp,
		// leaving no list here that could go stale.
		const build = (sw.names[0] || '').replace(/^blocks5-/, '');
		const need = ['/index.html', '/blocks5-' + build + '.js',
		              '/blocks5-' + build + '.wasm', '/blocks5-' + build + '.data'];
		const missing = need.filter(f => !sw.held.includes(f));
		if (sw.names.length === 1 && build && !missing.length) {
			ok('service worker: cache "' + sw.names[0] + '" holds all ' + sw.held.length + ' files');
		} else {
			bad('service worker cache ' + JSON.stringify(sw.names) + ' is missing ' + missing.join(', '));
		}
		// And nothing unstamped: that is exactly what broke it on a server
		// running mod_pagespeed.
		const unstamped = sw.held.filter(f => /^\/blocks5\.(js|wasm|data)$/.test(f));
		if (unstamped.length) bad('unstamped names in the cache: ' + unstamped.join(', '));
		else ok('every payload file carries the build stamp');

		// --- 6b. a new build gets through too --------------------------------
		// index.html is the one file with no stamp in its name; answering it from
		// the cache would keep a new version from ever becoming visible. Checked
		// with a marker the game does not touch: document.title will not do, since
		// the game sets that itself through SDL_WM_SetCaption.
		const indexFile = path.join(DIR, 'index.html');
		const original = fs.readFileSync(indexFile, 'utf8');
		try {
			fs.writeFileSync(indexFile, original.replace(
				'<title>Blocks 5</title>',
				'<title>Blocks 5</title><meta name="b5probe" content="new">'));
			await page.reload();
			await wait(2500);
			const probe = await page.evaluate(() => {
				const m = document.querySelector('meta[name=b5probe]');
				return m ? m.content : '';
			});
			if (probe === 'new') ok('a changed index.html arrives on an ordinary reload');
			else bad('index.html came from the cache - a new version would never become visible');
		} finally {
			fs.writeFileSync(indexFile, original);
		}
		await page.reload();
		await waitFor(page, booted, 'the restart', 240000);

		// --- 6c. the pad is stamped, and the page really loads it -----------
		// This is what replaced the header rule for touch_controls.js, and it is
		// the stronger mechanism: a stamped URL is immutable, so no cache
		// anywhere can go stale on it and no server has to be configured for it
		// to be true. The stamp is the pad's OWN hash and not the payload's -
		// reusing $version would not move when only the pad changed, and the file
		// would then be served immutable for a year, which is worse than the bug
		// it replaced. So: the page must name a stamped pad, that file must exist,
		// and it must have run.
		const indexText = fs.readFileSync(path.join(DIR, 'index.html'), 'utf8');
		const padRef = indexText.match(/touch_controls-([0-9a-f]+)\.js/);
		if (!padRef) {
			bad('index.html does not reference a stamped touch_controls-<hash>.js');
		} else if (/["'\/]touch_controls\.js/.test(indexText)) {
			bad('index.html still references the unstamped touch_controls.js');
		} else if (!fs.existsSync(path.join(DIR, padRef[0]))) {
			bad(padRef[0] + ' is named by the page but not in the build');
		} else {
			const ran = await page.evaluate(() => typeof window.b5_setPadLanguage === 'function');
			if (ran) ok('the pad is stamped (' + padRef[0] + ') and ran');
			else bad(padRef[0] + ' was served but did not run');
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
