// burst.js - a series of screenshots of the title demo, to catch a brief
// effect that only exists while Bob is walking over something.
//
//   B5_SHOTS=/tmp/burst node burst.js panel 45 400
//     name prefix, count, interval in milliseconds
//
// How to measure the pictures afterwards is in README.md under "Measuring an
// effect in the picture" - including the conversion from game to screenshot
// coordinates.
const h = require('./harness');

(async () => {
	const { browser, page } = await h.launch();
	await h.start(page);
	const n = parseInt(process.argv[3] || '30', 10);
	const ms = parseInt(process.argv[4] || '250', 10);
	for (let i = 0; i < n; i++) {
		await h.shot(page, (process.argv[2] || 'burst') + '-' + String(i).padStart(2, '0'));
		await page.waitForTimeout(ms);
	}
	await browser.close();
})().catch(e => { console.log('FEHLGESCHLAGEN: ' + e.message); process.exit(1); });
