// smoke.js - one round through the GUI, touching everything that is new in
// this version: the Manager, the options dialog with its key bindings, and the
// order in which Escape closes the panes.
//
//   cd WebBuild && ./build.sh hooks && cd test
//   NODE_PATH=/opt/node22/lib/node_modules node smoke.js

const h = require('./harness');
const fs = require('fs');
const os = require('os');
const path = require('path');
const zlib = require('zlib');

(async () => {
	const { browser, page } = await h.launch();

	await h.start(page);
	await h.expectState(page, 'GS_Menu');
	await h.shot(page, 'smoke-1-menu');

	// --- options: open, close again -------------------------------------------
	await h.clickPath(page, 'Menu.Options');
	await h.expectShown(page, 'OptionsPane.Options');
	await h.shot(page, 'smoke-2-options');

	// Without a selection in the list the four buttons below it are disabled.
	const opts = await h.dump(page);
	for (const name of ['OptionsPane.Options.PrimaryKey', 'OptionsPane.Options.SecondaryKey',
	                    'OptionsPane.Options.ResetSelected']) {
		const el = h.find(opts, name);
		if (el.active) h.note(name + ' ist ohne Auswahl bedienbar, sollte es aber nicht sein');
	}

	// Escape belongs to the dialog, not to the menu underneath - otherwise it
	// quits the game instead of closing the dialog.
	await h.key(page, 'Escape');
	await h.expectShown(page, 'OptionsPane.Options', false);
	await h.expectState(page, 'GS_Menu');

	// --- Manager: step through the four kinds ---------------------------------
	await h.clickPath(page, 'Menu.Manager');
	await h.expectShown(page, 'Menu.ManagerPane.Manager');
	for (const kind of ['KindLevel', 'KindCampaign', 'KindMusic', 'KindSkin']) {
		await h.clickPath(page, 'Menu.ManagerPane.Manager.' + kind);
	}
	await h.shot(page, 'smoke-3-manager');

	// On a fresh profile everything in these lists is shipped - and shipped
	// means: export yes, delete no. That is exactly the rule from
	// Transfer::isBuiltIn(), and from outside the only sign of it is a button
	// staying grey.
	for (const kind of ['KindLevel', 'KindCampaign', 'KindSkin']) {
		await h.clickPath(page, 'Menu.ManagerPane.Manager.' + kind);
		const mgr = await h.dump(page);
		if (h.find(mgr, 'Menu.ManagerPane.Manager.Delete').active) {
			h.note(kind + ': Loeschen ist bedienbar, obwohl nur Mitgeliefertes in der Liste steht');
		}
		if (!h.find(mgr, 'Menu.ManagerPane.Manager.Export').active) {
			h.note(kind + ': Ausgeben ist abgeschaltet, obwohl etwas ausgewaehlt ist');
		}
	}

	// The game ships no music of its own: the list is empty and both buttons
	// stay grey.
	await h.clickPath(page, 'Menu.ManagerPane.Manager.KindMusic');
	const music = await h.dump(page);
	for (const name of ['Menu.ManagerPane.Manager.Export', 'Menu.ManagerPane.Manager.Delete']) {
		if (h.find(music, name).active) h.note(name + ' ist bedienbar, obwohl die Liste leer ist');
	}

	// Escape closes the Manager and not the game.
	await h.key(page, 'Escape');
	await h.expectShown(page, 'Menu.ManagerPane.Manager', false);
	await h.expectState(page, 'GS_Menu');
	await h.shot(page, 'smoke-4-back');

	// A tab in the background must be silent. The game cannot see to that
	// itself: without requestAnimationFrame no logic tick runs, and Emscripten's
	// SDL reports a hidden page as SDL_WINDOWEVENT anyway, which the game does
	// not listen for. The page therefore suspends the AudioContext - without it
	// the music falls silent on its own once its queue runs dry, but a
	// continuous sound like the laser would keep going.
	//
	// Headless Chromium has no real tab visibility; bringToFront leaves
	// document.hidden at false. What is dispatched is therefore exactly the
	// event the browser sends when switching away.
	const setHidden = (value) => page.evaluate((v) => {
		Object.defineProperty(document, 'hidden', { configurable: true, get: () => v });
		Object.defineProperty(document, 'visibilityState',
		                      { configurable: true, get: () => (v ? 'hidden' : 'visible') });
		document.dispatchEvent(new Event('visibilitychange'));
	}, value);
	const audioState = () => page.evaluate(() => {
		try {
			const c = AL.currentCtx && AL.currentCtx.audioCtx;
			return c ? c.state : '(kein Kontext)';
		} catch (e) { return '(nicht erreichbar)'; }
	});

	const before = await audioState();
	if (before !== 'running') {
		h.note('vor dem Test steht der AudioContext auf "' + before + '", erwartet "running"');
	} else {
		await setHidden(true);
		await page.waitForTimeout(800);
		const hidden = await audioState();
		if (hidden !== 'suspended') h.note('verborgener Tab: AudioContext "' + hidden + '", erwartet "suspended"');

		await setHidden(false);
		await page.waitForTimeout(800);
		const shown = await audioState();
		if (shown !== 'running') h.note('sichtbarer Tab: AudioContext "' + shown + '", erwartet "running"');
	}

	// And the Engine itself must notice the focus change - in the browser SDL
	// reports it as SDL_WINDOWEVENT instead of SDL_ACTIVEEVENT, and without the
	// matching branch the game would keep running in the background instead of
	// pausing as it does everywhere else. Both routes there are checked.
	const appActive = async () => (await h.dump(page)).appActive;
	for (const [name, away, back] of [
		['blur/focus', () => window.dispatchEvent(new Event('blur')),
		               () => window.dispatchEvent(new Event('focus'))],
		['visibilitychange',
		 () => { Object.defineProperty(document, 'hidden', { configurable: true, get: () => true });
		         document.dispatchEvent(new Event('visibilitychange')); },
		 () => { Object.defineProperty(document, 'hidden', { configurable: true, get: () => false });
		         document.dispatchEvent(new Event('visibilitychange')); }],
	]) {
		await page.evaluate(away);
		await page.waitForTimeout(1200);
		if (await appActive()) h.note(name + ': die Engine haelt sich noch fuer aktiv');
		await page.evaluate(back);
		await page.waitForTimeout(1200);
		if (!(await appActive())) h.note(name + ': die Engine kommt nicht zurueck');
	}
	await h.expectState(page, 'GS_Menu');

	// --- screenshot -----------------------------------------------------------
	// F11 has no directory here to write to; the picture lands in the player's
	// downloads. The whole path is checked: that the browser gets a download at
	// all, and that it holds a PNG produced by the game's own encoder
	// (src/img_save.cpp).
	const download = new Promise(res => page.once('download', d => res(d)));
	await h.key(page, 'F11');
	const shot = await Promise.race([download,
	                                 new Promise(r => setTimeout(() => r(null), 8000))]);
	if (!shot) {
		h.note('F11 hat keinen Download ausgeloest');
	} else {
		const file = path.join(os.tmpdir(), 'blocks5-webshot.png');
		await shot.saveAs(file);
		const png = fs.readFileSync(file);
		if (!/^blocks5_.*\.png$/.test(shot.suggestedFilename())) {
			h.note('Bildschirmfoto heisst "' + shot.suggestedFilename() + '"');
		}
		// Signature, IHDR and an IDAT that unpacks to exactly (width*3+1)*height
		// bytes - that is what catches a truncated file; its size alone does not.
		if (png.slice(0, 8).toString('hex') !== '89504e470d0a1a0a') {
			h.note('Bildschirmfoto hat keine PNG-Signatur');
		} else {
			const w = png.readUInt32BE(16), hgt = png.readUInt32BE(20);
			const type = png.slice(12, 16).toString();
			const idatLen = png.readUInt32BE(16 + 13 + 4);
			const idat = png.slice(16 + 13 + 12, 16 + 13 + 12 + idatLen);
			const raw = zlib.inflateSync(idat).length;
			if (type !== 'IHDR' || w !== 640 || hgt !== 480 || png[24] !== 8 || png[25] !== 2) {
				h.note('Bildschirmfoto: ' + type + ' ' + w + 'x' + hgt +
				       ' bd=' + png[24] + ' ct=' + png[25]);
			} else if (raw !== (w * 3 + 1) * hgt) {
				h.note('Bildschirmfoto: IDAT entpackt zu ' + raw + ' statt ' +
				       ((w * 3 + 1) * hgt) + ' Byte');
			} else {
				console.log('  . F11 hat ein gueltiges PNG heruntergeladen (' +
				            png.length + ' Byte)');
			}
		}
	}

	process.exit(await h.finish(browser));
})().catch(async (e) => {
	console.log('FEHLGESCHLAGEN: ' + e.message);
	process.exit(1);
});
