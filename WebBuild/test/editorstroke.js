// editorstroke.js - does the level editor draw a line to wherever the pointer
// last was?
//
//   WebBuild/build.sh hooks && node WebBuild/test/editorstroke.js
//
// The editor interpolates between the previous cursor cell and this one so
// that a fast drag leaves a continuous stroke. With a mouse the button-less
// moves between two strokes keep that previous cell under the pointer; a
// finger makes no such moves, so the first move of a new stroke can
// interpolate all the way back to where the last one ended.
//
// A finger cannot be had here, but the shape can: CDP dispatches a
// mousePressed at a point with no mouseMoved before it, which is exactly what
// a touch delivers. page.mouse always moves first and can never reproduce it -
// which is also why the bug is invisible with a real mouse.
//
// It reads the answer off the picture, because a stroke across the level is
// what the bug looks like and there is no hook that reports tiles: the two
// strokes are drawn far apart, and the middle of the field between them must
// stay as empty as it was.
const h = require('./harness.js');
const { execFileSync } = require('child_process');

const A = { x: 80, y: 80 };          // first stroke, in the 640x480 game frame
const B = { x: 560, y: 330 };        // second, far away in both axes
const DRAW = 48;                     // how far each stroke drags

async function toPage(page, present, gx, gy) {
	const box = await page.evaluate(() => {
		const c = Module.canvas, r = c.getBoundingClientRect();
		return { left: r.left, top: r.top, cssW: r.width, cssH: r.height, w: c.width, h: c.height };
	});
	const wx = present[0] + gx * present[2] / 640;
	const wy = present[1] + gy * present[3] / 480;
	return { x: box.left + wx * (box.cssW / box.w), y: box.top + wy * (box.cssH / box.h) };
}

async function stroke(cdp, page, present, from, moveFirst) {
	const p0 = await toPage(page, present, from.x, from.y);
	const p1 = await toPage(page, present, from.x + DRAW, from.y);

	// moveFirst is what a mouse does and a finger does not.
	if (moveFirst) {
		await cdp.send('Input.dispatchMouseEvent', { type: 'mouseMoved', x: p0.x, y: p0.y, buttons: 0 });
		await page.waitForTimeout(300);
	}
	await cdp.send('Input.dispatchMouseEvent',
	               { type: 'mousePressed', x: p0.x, y: p0.y, button: 'left', buttons: 1, clickCount: 1 });
	await page.waitForTimeout(300);
	await cdp.send('Input.dispatchMouseEvent',
	               { type: 'mouseMoved', x: p1.x, y: p1.y, button: 'left', buttons: 1 });
	await page.waitForTimeout(300);
	await cdp.send('Input.dispatchMouseEvent',
	               { type: 'mouseReleased', x: p1.x, y: p1.y, button: 'left', buttons: 0, clickCount: 1 });
	await page.waitForTimeout(300);
}

// How much of the field is painted. The screenshot goes through
// count_painted.py rather than being read here: the canvas is WebGL without
// preserveDrawingBuffer, so drawImage after a frame hands back an empty image.
async function paintedCells(page, present, name) {
	const file = (process.env.B5_SHOTS || '/tmp') + '/' + name + '.png';
	await page.screenshot({ path: file });
	const canvasW = await page.evaluate(() => Module.canvas.width);
	const out = execFileSync('python3', [__dirname + '/count_painted.py', file, String(canvasW),
	                                     String(present[0]), String(present[1]),
	                                     String(present[2]), String(present[3])]);
	return parseInt(out.toString().trim(), 10);
}

(async () => {
	const { browser, page } = await h.launch({ dir: __dirname + '/../build-test' });
	const cdp = await page.context().newCDPSession(page);

	await h.start(page);

	await h.clickPath(page, 'Menu.LevelEditor');
	await h.waitFor(page, d => d.state === 'GS_LevelEditor', 'the level editor', 90000);
	await page.waitForTimeout(2000);

	const present = (await h.dump(page)).present;
	const before = await paintedCells(page, present, 'editor-0-empty');
	console.log('  empty field:                  ' + before + ' cells painted');

	// A normal stroke, the way a mouse delivers it.
	await stroke(cdp, page, present, A, true);
	const afterA = await paintedCells(page, present, 'editor-1-strokeA');
	console.log('  after stroke A (mouse-like):  ' + afterA + ' cells painted');

	// The same again far away, pressed with no move before it - the finger.
	await stroke(cdp, page, present, B, false);
	const afterB = await paintedCells(page, present, 'editor-2-strokeB');
	console.log('  after stroke B (finger-like): ' + afterB + ' cells painted');

	const drewA = afterA - before;
	const drewB = afterB - afterA;
	console.log('  stroke A painted ' + drewA + ' cells, stroke B painted ' + drewB);

	if (drewA < 2) h.note('stroke A drew nothing - the test never reached the field');
	// A stroke of 48 px is four cells. Anything near the 30+ cells between A
	// and B is the interpolation running back to the end of the last stroke.
	else if (drewB > drewA * 3) h.note('stroke B painted ' + drewB + ' cells for a ' +
	                                   Math.round(DRAW / 16) + '-cell drag - it drew a line back to stroke A');
	else console.log('  OK: stroke B stayed local');

	await h.finish(browser);
})().catch(e => { console.error(e); process.exit(1); });
