// perf.js - what a frame costs in the browser, and whether a change to it is real.
//
//   WebBuild/build.sh hooks && node WebBuild/test/perf.js
//   B5_WINDOW=30 B5_REPEATS=5 node WebBuild/test/perf.js
//   node WebBuild/test/perf.js "" "?texunits=1" "?texunits=4"
//
// Every number is milliseconds of wall clock on the main thread, from
// FrameStats in the game. None of it waits for the GPU - WebGL takes a command
// and returns - so this measures what the emulation and the JavaScript cost.
// That is the half worth measuring here: it is the main thread that starves
// the audio and drops the frame, and it is the half a setting like
// GL_MAX_TEXTURE_IMAGE_UNITS can move at all.
//
// The arms are query strings rather than builds, so both sides of a comparison
// are the same binary in the same browser, and they are interleaved rather than
// run in blocks, so a machine that warms up or throttles part-way through
// spreads that over both instead of handing it to one.
//
// The scene is the menu's own title demo: a whole level animating, plus the
// GUI, with no navigation to go wrong. It is not deterministic - bombs go off
// when they go off - which is why this reports every repeat and the spread
// between them, and not one number with an air of authority.
const h = require('./harness.js');

// Both written out, so the labels say what was run: an empty query is now
// texunits=1, and texunits=0 is what the emulation does when nobody tells it.
const ARMS = process.argv.length > 2 ? process.argv.slice(2) : ['?texunits=0', '?texunits=1'];
const REPEATS = parseInt(process.env.B5_REPEATS || '3', 10);
const WINDOW = parseInt(process.env.B5_WINDOW || '20', 10);   // seconds per run

const name = arm => (arm === '' ? 'default' : arm.replace(/^\?/, ''));

async function measure(page, arm) {
	await h.boot(page, arm, 150000);
	await h.start(page);
	// The first frames after a state change are not the scene: textures are
	// still being uploaded and the crossfade is still running.
	await page.waitForTimeout(3000);

	await h.resetStats(page);
	await page.waitForTimeout(WINDOW * 1000);
	const d = await h.dump(page);
	if (!d.frames) throw new Error('the dump carries no frame timings - is this a hooks build?');
	return d.frames;
}

const median = xs => {
	const s = xs.slice().sort((a, b) => a - b);
	return s.length % 2 ? s[(s.length - 1) / 2] : (s[s.length / 2 - 1] + s[s.length / 2]) / 2;
};

(async () => {
	const { browser, page } = await h.launch({ bootTimeout: 150000 });
	const runs = {};
	for (const arm of ARMS) runs[arm] = [];

	// Interleaved: repeat by repeat, not arm by arm.
	for (let r = 0; r < REPEATS; r++) {
		for (const arm of ARMS) {
			const f = await measure(page, arm);
			runs[arm].push(f);
			console.log('  run ' + (r + 1) + '  ' + name(arm).padEnd(12) +
			            '  frames ' + String(f.count).padStart(4) +
			            '   total p50 ' + f.total[0].toFixed(2) +
			            '  p95 ' + f.total[1].toFixed(2) +
			            '   render p50 ' + f.render[0].toFixed(2) +
			            '   present p50 ' + f.present[0].toFixed(2));
		}
	}

	const PHASES = ['total', 'render', 'update', 'present', 'interval'];
	console.log('\n  median over ' + REPEATS + ' runs of ' + WINDOW + ' s, p50 of each run (ms):');
	console.log('  ' + 'arm'.padEnd(12) + PHASES.map(p => p.padStart(10)).join(''));
	const summary = {};
	for (const arm of ARMS) {
		summary[arm] = {};
		let row = '  ' + name(arm).padEnd(12);
		for (const p of PHASES) {
			const v = median(runs[arm].map(f => f[p][0]));
			summary[arm][p] = v;
			row += v.toFixed(2).padStart(10);
		}
		console.log(row);
	}

	// Whether a difference means anything: compare it against how far the
	// repeats of a single arm are already apart. A change smaller than the
	// noise of one arm is not a result.
	if (ARMS.length > 1) {
		console.log('\n  against "' + name(ARMS[0]) + '":');
		const base = ARMS[0];
		for (const p of PHASES) {
			const spread = Math.max(...ARMS.map(a => {
				const xs = runs[a].map(f => f[p][0]);
				return Math.max(...xs) - Math.min(...xs);
			}));
			let row = '  ' + p.padEnd(12);
			for (const arm of ARMS.slice(1)) {
				const d = summary[arm][p] - summary[base][p];
				const pct = summary[base][p] ? (100 * d / summary[base][p]) : 0;
				row += (d >= 0 ? '+' : '') + d.toFixed(2) + ' ms (' +
				       (pct >= 0 ? '+' : '') + pct.toFixed(1) + '%)  ';
			}
			row += ' spread within an arm: ' + spread.toFixed(2) + ' ms';
			if (Math.abs(summary[ARMS[1]][p] - summary[base][p]) < spread) row += '  <- under the noise';
			console.log(row);
		}
	}

	await h.finish(browser);
})().catch(e => { console.error(e); process.exit(1); });
