// harness.js - drive the hooks build of Blocks 5 from Playwright by element
// name instead of by pixel.
//
// The game is one canvas: Playwright can only click a screen coordinate, and
// working that coordinate out from a screenshot is where these tests kept
// going wrong. test_hooks.cpp answers instead - it reports every GUI element
// with its window rectangle and, for each, whether a click on its centre would
// really arrive there. clickPath() looks the element up by name and then does
// an ordinary mouse click on the right spot, so the input still travels the
// whole way through SDL, Engine and GUI.
//
// Needs a hooks build:  ./build.sh hooks    (serves from WebBuild/build-test)
//
//   const h = require('./harness');
//   const { browser, page } = await h.launch({ dir: __dirname + '/../build-test' });
//   await h.start(page);                      // click-to-start, dismiss the CRT offer
//   await h.clickPath(page, 'Menu.Options');
//   await h.expectShown(page, 'Options');
//   await h.shot(page, 'options');
//   await h.finish(browser);

const { chromium } = require('playwright');
const { spawn } = require('child_process');
const path = require('path');
const fs = require('fs');

const CHROME = '/opt/pw-browsers/chromium-1194/chrome-linux/chrome';
const PORT = 8099;

let server = null;
const problems = [];

// The game boots slowly under swiftshader: wasm, then a 13 MB preload, then the
// loading state's own progress bar. Poll for the dump instead of sleeping.
async function waitForDump(page, timeoutMs) {
	const deadline = Date.now() + (timeoutMs || 90000);
	for (;;) {
		const dump = await page.evaluate(() => {
			// runtimeInitialized, not Module.calledRun: the latter is a closure
			// variable in this Emscripten and never appears on Module, and calling
			// a kept-alive export before the wasm is instantiated hits a stub that
			// aborts - which hangs the page rather than throwing.
			if (typeof Module === 'undefined') return null;
			if (typeof runtimeInitialized === 'undefined' || !runtimeInitialized) return null;
			if (!Module._blocks5_testDump) return null;
			try { Module._blocks5_testDump(); } catch (e) { return null; }
			return Module.b5_test || null;
		});
		if (dump) {
			// The runtime can be up before main() has built the GUI; an empty tree
			// means "too early", not "ready".
			const d = JSON.parse(dump);
			if (d.state && d.elements && d.elements.length) return d;
		}
		if (Date.now() > deadline) {
			throw new Error('the game did not come up, or this is not a hooks build ' +
			                '(build with ./build.sh hooks and serve WebBuild/build-test)');
		}
		await page.waitForTimeout(500);
	}
}

async function serve(dir) {
	if (server) return;
	if (!fs.existsSync(path.join(dir, 'blocks5.html'))) {
		throw new Error('no blocks5.html in ' + dir);
	}
	server = spawn('python3', ['-m', 'http.server', String(PORT)],
	               { cwd: dir, stdio: 'ignore', detached: true });
	await new Promise(r => setTimeout(r, 1500));
}

async function launch(opts) {
	const o = opts || {};
	const dir = o.dir || path.join(__dirname, '..', 'build-test');
	await serve(dir);

	const browser = await chromium.launch({
		executablePath: CHROME,
		args: ['--use-gl=swiftshader', '--enable-unsafe-swiftshader', '--no-sandbox'],
	});
	const page = await browser.newPage({
		viewport: { width: o.width || 800, height: o.height || 640 },
	});
	// Three notices always appear and mean nothing: Emscripten announcing its GL
	// emulation twice, and the game reporting that a browser has no loopback
	// audio device to record from.
	const EXPECTED = [
		/using emscripten GL/i,
		/Could not open audio capture device/i,
	];
	page.on('console', m => {
		const t = m.text();
		if (EXPECTED.some(re => re.test(t))) return;
		if (/error|abort|assertion|exception|warning/i.test(t)) problems.push('console: ' + t);
	});
	page.on('pageerror', e => problems.push('pageerror: ' + e.message));

	await page.goto('http://127.0.0.1:' + PORT + '/blocks5.html');
	await waitForDump(page, o.bootTimeout);
	return { browser, page };
}

async function dump(page) {
	const json = await page.evaluate(() => {
		Module._blocks5_testDump();
		return Module.b5_test;
	});
	return JSON.parse(json);
}

function find(d, pathName) {
	const hits = d.elements.filter(e => e.path === pathName);
	if (hits.length === 0) {
		const near = d.elements
			.filter(e => e.path.indexOf(pathName.split('.').pop()) >= 0)
			.map(e => e.path).slice(0, 8);
		throw new Error('no element "' + pathName + '"' +
		                (near.length ? ' - did you mean: ' + near.join(', ') : ''));
	}
	if (hits.length > 1) throw new Error('"' + pathName + '" appears ' + hits.length + ' times');
	return hits[0];
}

// The canvas is laid out by CSS and its backing store is a different size, so
// the window coordinates the dump reports have to be scaled into page space.
async function toPage(page, win) {
	const box = await page.evaluate(() => {
		const c = Module.canvas;
		const r = c.getBoundingClientRect();
		return { left: r.left, top: r.top, cssW: r.width, cssH: r.height, w: c.width, h: c.height };
	});
	return {
		x: box.left + (win[0] + win[2] / 2) * (box.cssW / box.w),
		y: box.top + (win[1] + win[3] / 2) * (box.cssH / box.h),
	};
}

async function clickPath(page, pathName, opts) {
	const o = opts || {};
	const d = await dump(page);
	const el = find(d, pathName);

	if (!el.shown) throw new Error(pathName + ' is not visible');
	if (!el.active && !o.allowInactive) throw new Error(pathName + ' is greyed out');
	if (d.crt) problems.push('the CRT filter is on; window coordinates ignore its curvature');

	// Ask the game who would get a click on that spot. This is the check that
	// makes the whole thing worth having: a rectangle is not the same as being
	// reachable, and a pane drawn on top is exactly what a screenshot hides.
	if (!o.allowCovered) {
		const cx = el.rect[0] + Math.floor(el.rect[2] / 2);
		const cy = el.rect[1] + Math.floor(el.rect[3] / 2);
		const hit = await page.evaluate(([x, y]) => {
			Module.b5_hit = '';
			Module._blocks5_testHitAt(x, y);
			return Module.b5_hit;
		}, [cx, cy]);
		if (hit !== pathName) {
			throw new Error('a click on the middle of ' + pathName + ' would go to ' +
			                (hit ? '"' + hit + '"' : 'nothing') +
			                ' - something is on top, or it has no hit area');
		}
	}

	const p = await toPage(page, el.win);
	await press(page, p.x, p.y, o);
	await page.waitForTimeout(o.wait || 800);
}

// Not page.mouse.click(): that presses and releases in the same millisecond,
// and the game samples the mouse once per logic tick. A press that goes down
// and up inside one tick is never seen as a click - the button stays unpressed
// and the test looks like a missed coordinate. So move, settle, hold, release.
async function press(page, x, y, opts) {
	const o = opts || {};
	await page.mouse.move(x, y);
	await page.waitForTimeout(o.settleMs || 200);
	await page.mouse.down();
	await page.waitForTimeout(o.holdMs || 400);
	await page.mouse.up();
}

async function key(page, name, opts) {
	const o = opts || {};
	if (o.holdMs) {
		await page.keyboard.down(name);
		await page.waitForTimeout(o.holdMs);
		await page.keyboard.up(name);
	} else {
		await page.keyboard.press(name);
	}
	await page.waitForTimeout(o.wait || 600);
}

async function expectShown(page, pathName, want) {
	const el = find(await dump(page), pathName);
	const shown = !!el.shown;
	const expected = (want === undefined) ? true : !!want;
	if (shown !== expected) {
		problems.push(pathName + ': visible=' + shown + ', expected ' + expected);
	}
	return shown;
}

async function expectState(page, want) {
	const d = await dump(page);
	if (d.state !== want) problems.push('game state is ' + d.state + ', expected ' + want);
	return d.state;
}

async function shot(page, name) {
	const dir = process.env.B5_SHOTS || '/tmp';
	await page.screenshot({ path: path.join(dir, name + '.png') });
}

// Wait until the game reaches a state the caller can work with. Under
// swiftshader a frame takes a fifth of a second, so every "it should be there
// by now" guess is wrong; ask instead.
async function waitFor(page, predicate, what, timeoutMs) {
	const deadline = Date.now() + (timeoutMs || 60000);
	for (;;) {
		const d = await dump(page);
		if (predicate(d)) return d;
		if (Date.now() > deadline) throw new Error('timed out waiting for ' + what);
		await page.waitForTimeout(500);
	}
}

const shown = (d, path) => d.elements.some(e => e.path === path && e.shown);

// The title screen waits for a click, and a fresh profile is met by the CRT
// offer. Both are in the way of every test, so get them out of the way here.
async function start(page) {
	const d = await dump(page);
	const p = await toPage(page, [0, 0, d.display[2], d.display[3]]);
	await press(page, p.x, p.y);

	await waitFor(page, x => x.state === 'GS_Menu', 'the main menu', 90000);

	if (shown(await dump(page), 'Menu.CrtPane')) {
		await clickPath(page, 'Menu.CrtPane.Crt.NoThanks');
		await waitFor(page, x => !shown(x, 'Menu.CrtPane'), 'the CRT offer to close');
	}
}

function report() {
	if (problems.length === 0) {
		console.log('OK');
		return 0;
	}
	console.log(problems.length + ' problem(s):');
	problems.forEach(p => console.log('  - ' + p));
	return 1;
}

async function finish(browser) {
	if (browser) await browser.close();
	if (server) { try { process.kill(-server.pid); } catch (e) {} server = null; }
	const code = report();
	process.exitCode = code;
	return code;
}

function note(text) { problems.push(text); }

module.exports = { launch, dump, find, clickPath, key, expectShown, expectState,
                   shot, start, waitFor, shown, press, finish, note, problems };
