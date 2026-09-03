// smoke.js - eine Runde durch die Oberflaeche, die alles anfasst, was in
// dieser Fassung neu ist: den Manager, den Optionsdialog samt Tastenbelegung,
// und die Reihenfolge, in der Escape die Fenster schliesst.
//
//   cd WebBuild && ./build.sh hooks && cd test
//   NODE_PATH=/opt/node22/lib/node_modules node smoke.js

const h = require('./harness');

(async () => {
	const { browser, page } = await h.launch();

	await h.start(page);
	await h.expectState(page, 'GS_Menu');
	await h.shot(page, 'smoke-1-menu');

	// --- Optionen: aufmachen, wieder zu ---------------------------------------
	await h.clickPath(page, 'Menu.Options');
	await h.expectShown(page, 'OptionsPane.Options');
	await h.shot(page, 'smoke-2-options');

	// Ohne Auswahl in der Liste sind die vier Knoepfe darunter abgeschaltet.
	const opts = await h.dump(page);
	for (const name of ['OptionsPane.Options.PrimaryKey', 'OptionsPane.Options.SecondaryKey',
	                    'OptionsPane.Options.ResetSelected']) {
		const el = h.find(opts, name);
		if (el.active) h.note(name + ' ist ohne Auswahl bedienbar, sollte es aber nicht sein');
	}

	// Escape gehoert dem Dialog, nicht dem Menue darunter - sonst beendet es das
	// Spiel, statt den Dialog zu schliessen.
	await h.key(page, 'Escape');
	await h.expectShown(page, 'OptionsPane.Options', false);
	await h.expectState(page, 'GS_Menu');

	// --- Manager: die vier Arten durchschalten --------------------------------
	await h.clickPath(page, 'Menu.Manager');
	await h.expectShown(page, 'Menu.ManagerPane.Manager');
	for (const kind of ['KindLevel', 'KindCampaign', 'KindMusic', 'KindSkin']) {
		await h.clickPath(page, 'Menu.ManagerPane.Manager.' + kind);
	}
	await h.shot(page, 'smoke-3-manager');

	// Auf einem frischen Profil ist alles, was in diesen Listen steht,
	// mitgeliefert - und mitgeliefert heisst: ausgeben ja, loeschen nein.
	// Genau das ist die Regel aus Transfer::isBuiltIn(), und sie ist von
	// aussen sonst nur daran zu erkennen, dass ein Knopf grau bleibt.
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

	// Musik bringt das Spiel keine mit, die Liste ist also leer und beide
	// Knoepfe bleiben grau.
	await h.clickPath(page, 'Menu.ManagerPane.Manager.KindMusic');
	const music = await h.dump(page);
	for (const name of ['Menu.ManagerPane.Manager.Export', 'Menu.ManagerPane.Manager.Delete']) {
		if (h.find(music, name).active) h.note(name + ' ist bedienbar, obwohl die Liste leer ist');
	}

	// Escape schliesst den Manager und nicht das Spiel.
	await h.key(page, 'Escape');
	await h.expectShown(page, 'Menu.ManagerPane.Manager', false);
	await h.expectState(page, 'GS_Menu');
	await h.shot(page, 'smoke-4-back');

	// Ein Tab im Hintergrund muss still sein. Das Spiel kann sich darum nicht
	// selbst kuemmern: ohne requestAnimationFrame laeuft kein Logiktakt, und
	// Emscriptens SDL meldet eine verborgene Seite ohnehin als SDL_WINDOWEVENT,
	// wonach das Spiel gar nicht horcht. Die Seite haelt deshalb den
	// AudioContext an - sonst stirbt zwar die Musik von selbst, wenn ihre
	// Warteschlange leerlaeuft, aber ein Dauerton wie der Laser liefe weiter.
	//
	// Headless-Chromium kennt keine echte Tab-Sichtbarkeit; bringToFront laesst
	// document.hidden auf false. Geschickt wird deshalb genau das Ereignis, das
	// der Browser beim Wegschalten schickt.
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
	await h.expectState(page, 'GS_Menu');

	process.exit(await h.finish(browser));
})().catch(async (e) => {
	console.log('FEHLGESCHLAGEN: ' + e.message);
	process.exit(1);
});
