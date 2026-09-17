// perf.js - what a frame costs in the browser, and whether a change to it is real.
//
//   WebBuild/build.sh hooks && node WebBuild/test/perf.js
//   B5_WINDOW=30 B5_REPEATS=5 node WebBuild/test/perf.js
//   node WebBuild/test/perf.js "" "?flushall=1"
//   B5_DIR=/somewhere/build-test B5_STALE_OK=1 node WebBuild/test/perf.js
//
// Every number is milliseconds of wall clock on the main thread, from
// FrameStats in the game. None of it waits for the GPU - WebGL takes a command
// and returns - so this measures what the game and the JavaScript around it
// cost. That is the half worth measuring here: it is the main thread that
// starves the audio and drops the frame, and it is the half a change to the
// game's own code can move at all.
//
// The arms are query strings rather than builds, so both sides of a comparison
// are the same binary in the same browser, and they are interleaved rather than
// run in blocks, so a machine that warms up or throttles part-way through
// spreads that over both instead of handing it to one. Two builds - a before
// and an after - are the one comparison that cannot be arms: run this once
// against each with B5_REPEATS=1, alternating, and B5_DIR names the other
// build (harness.js says why it needs B5_STALE_OK beside it).
//
// The scene is the menu's own title demo: a whole level animating, plus the
// GUI, with no navigation to go wrong. It is not deterministic - bombs go off
// when they go off - which is why this reports every repeat and the spread
// between them, and not one number with an air of authority.
const h = require('./harness.js');

// One arm unless told otherwise: the build as it is. Two or more and the last
// section says whether they differ by more than the noise.
const ARMS = process.argv.length > 2 ? process.argv.slice(2) : [''];
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
	// The draw-call and batch counters come along, because a render change
	// usually moves those first and the milliseconds only as a consequence -
	// and because both are accumulated over the same window as the timings.
	// draws is what reached WebGL per rendered frame (harness.js counts it on
	// the context); the batch's own draws are the renderer's flushes, a part
	// of that.
	if (d.draws) d.frames.drawsPerFrame = d.draws.calls / Math.max(d.draws.frames, 1);
	return { frames: d.frames, batch: d.batch };
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
			const m = await measure(page, arm);
			const f = m.frames;
			runs[arm].push(f);
			const b = m.batch;
			console.log('  run ' + (r + 1) + '  ' + name(arm).padEnd(12) +
			            '  frames ' + String(f.count).padStart(4) +
			            '   total p50 ' + f.total[0].toFixed(2) +
			            '  p95 ' + f.total[1].toFixed(2) +
			            '   render p50 ' + f.render[0].toFixed(2) +
			            '   present p50 ' + f.present[0].toFixed(2) +
			            (f.drawsPerFrame !== undefined ? '   ' + f.drawsPerFrame.toFixed(1) + ' draw calls/frame' : '') +
			            (b ? '   batch ' + (b.draws / Math.max(f.count, 1)).toFixed(1) + ' draws/frame' +
			                 '  ' + (b.quads / Math.max(b.draws, 1)).toFixed(1) + ' quads/draw' : ''));
		}
	}

	const PHASES = ['total', 'render', 'update', 'present', 'swap', 'interval'];
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
	// Draw calls beside the milliseconds: the count a renderer change moves
	// first, and one that does not wobble with the machine.
	for (const arm of ARMS) {
		const xs = runs[arm].map(f => f.drawsPerFrame).filter(x => x !== undefined);
		if (xs.length) console.log('  ' + name(arm).padEnd(12) + median(xs).toFixed(1) + ' draw calls/frame (median)');
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
