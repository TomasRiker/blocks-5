// burst.js - eine Reihe von Bildschirmfotos der Titeldemo, um einen kurzen
// Effekt zu erwischen, den es nur gibt, wenn Bob gerade ueber etwas laeuft.
//
//   B5_SHOTS=/tmp/burst node burst.js panel 45 400
//     Name-Praefix, Anzahl, Abstand in Millisekunden
//
// Wie man die Bilder danach ausmisst, steht in README.md unter "Einen Effekt
// im Bild nachmessen" - samt der Umrechnung von Spiel- in Bildkoordinaten.
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
